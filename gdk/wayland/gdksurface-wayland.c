/*
 * Copyright © 2010 Intel Corporation
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

#include "config.h"

#include "gdksurface-wayland.h"

#include "gdkdeviceprivate.h"
#include "gdkdisplay-wayland.h"
#include "gdkdragsurfaceprivate.h"
#include "gdkeventsprivate.h"
#include "gdkframeclockidleprivate.h"
#include "gdkglcontext-wayland.h"
#include "gdkmonitor-wayland.h"
#include "gdkpopupprivate.h"
#include "gdkprivate-wayland.h"
#include "gdkprivate-wayland.h"
#include "gdkseat-wayland.h"
#include "gdksurfaceprivate.h"
#include "gdktoplevelprivate.h"
#include "gdkdevice-wayland-private.h"

#include <wayland/xdg-shell-unstable-v6-client-protocol.h>
#include <wayland/xdg-foreign-unstable-v2-client-protocol.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <netinet/in.h>
#include <unistd.h>

/**
 * GdkWaylandSurface:
 *
 * The Wayland implementation of `GdkSurface`.
 *
 * Beyond the [class@Gdk.Surface] API, the Wayland implementation offers
 * access to the Wayland `wl_surface` object with
 * [method@GdkWayland.WaylandSurface.get_wl_surface].
 */

/**
 * GdkWaylandToplevel:
 *
 * The Wayland implementation of `GdkToplevel`.
 *
 * Beyond the [iface@Gdk.Toplevel] API, the Wayland implementation
 * has API to set up cross-process parent-child relationships between
 * surfaces with [method@GdkWayland.WaylandToplevel.export_handle] and
 * [method@GdkWayland.WaylandToplevel.set_transient_for_exported].
 */

/**
 * GdkWaylandPopup:
 *
 * The Wayland implementation of `GdkPopup`.
 */

#define SURFACE_IS_TOPLEVEL(surface)  TRUE

#define MAX_WL_BUFFER_SIZE (4083) /* 4096 minus header, string argument length and NUL byte */

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
    /* The wl_outputs that this surface currently touches */
    GSList               *outputs;

    struct wl_surface    *wl_surface;

    struct xdg_surface *xdg_surface;
    struct xdg_toplevel *xdg_toplevel;
    struct xdg_popup *xdg_popup;

    /* Legacy xdg-shell unstable v6 fallback support */
    struct zxdg_surface_v6 *zxdg_surface_v6;
    struct zxdg_toplevel_v6 *zxdg_toplevel_v6;
    struct zxdg_popup_v6 *zxdg_popup_v6;

    struct gtk_surface1  *gtk_surface;
    struct wl_egl_window *egl_window;
  } display_server;

  struct wl_event_queue *event_queue;

  uint32_t reposition_token;
  uint32_t received_reposition_token;

  PopupState popup_state;

  unsigned int popup_thaw_upon_show : 1;
  unsigned int initial_configure_received : 1;
  unsigned int has_uncommitted_ack_configure : 1;
  unsigned int mapped : 1;
  unsigned int awaiting_frame : 1;
  unsigned int awaiting_frame_frozen : 1;

  int pending_buffer_offset_x;
  int pending_buffer_offset_y;

  char *title;

  struct {
    gboolean was_set;

    char *application_id;
    char *app_menu_path;
    char *menubar_path;
    char *window_object_path;
    char *application_object_path;
    char *unique_bus_name;
  } application;

  GdkGeometry geometry_hints;
  GdkSurfaceHints geometry_mask;

  GdkSeat *grab_input_seat;

  gint64 pending_frame_counter;
  guint32 scale;

  int shadow_left;
  int shadow_right;
  int shadow_top;
  int shadow_bottom;

  struct wl_output *initial_fullscreen_output;

  cairo_region_t *opaque_region;
  gboolean opaque_region_dirty;

  cairo_region_t *input_region;
  gboolean input_region_dirty;

  GdkRectangle last_sent_window_geometry;
  int last_sent_min_width;
  int last_sent_min_height;
  int last_sent_max_width;
  int last_sent_max_height;

  int saved_width;
  int saved_height;

  struct {
    GdkToplevelLayout *layout;

    int bounds_width;
    int bounds_height;
    gboolean has_bounds;
  } toplevel;

  struct {
    GdkPopupLayout *layout;
    int unconstrained_width;
    int unconstrained_height;
  } popup;

  struct {
    struct {
      int width;
      int height;
      GdkToplevelState state;
      gboolean is_resizing;

      int bounds_width;
      int bounds_height;
      gboolean has_bounds;
    } toplevel;

    struct {
      int x;
      int y;
      int width;
      int height;
      uint32_t repositioned_token;
      gboolean has_repositioned_token;
    } popup;

    gboolean is_initial_configure;

    uint32_t serial;
    gboolean is_dirty;
  } pending;

  struct {
    GdkToplevelState unset_flags;
    GdkToplevelState set_flags;
  } initial_state;

  struct {
    struct {
      gboolean should_constrain;
      gboolean size_is_fixed;
    } toplevel;
    struct {
      int x;
      int y;
    } popup;
    int configured_width;
    int configured_height;
    gboolean surface_geometry_dirty;
  } next_layout;

  uint32_t last_configure_serial;

  int state_freeze_count;

  struct zxdg_imported_v1 *imported_transient_for;
  GHashTable *shortcuts_inhibitors;
};

typedef struct _GdkWaylandSurfaceClass GdkWaylandSurfaceClass;
struct _GdkWaylandSurfaceClass
{
  GdkSurfaceClass parent_class;
};

G_DEFINE_TYPE (GdkWaylandSurface, gdk_wayland_surface, GDK_TYPE_SURFACE)

struct _GdkWaylandToplevel
{
  GdkWaylandSurface parent_instance;

  GdkWaylandToplevel *transient_for;

  struct org_kde_kwin_server_decoration *server_decoration;
  struct zxdg_exported_v1 *xdg_exported;
  struct zxdg_exported_v2 *xdg_exported_v2;

  struct {
    GdkWaylandToplevelExported callback;
    gpointer user_data;
    GDestroyNotify destroy_func;
  } exported;

  struct zwp_idle_inhibitor_v1 *idle_inhibitor;
  size_t idle_inhibitor_refcount;
};

typedef struct
{
  GdkWaylandSurfaceClass parent_class;
} GdkWaylandToplevelClass;

static void gdk_wayland_toplevel_iface_init (GdkToplevelInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GdkWaylandToplevel, gdk_wayland_toplevel, GDK_TYPE_WAYLAND_SURFACE,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_TOPLEVEL,
                                                gdk_wayland_toplevel_iface_init))

struct _GdkWaylandPopup
{
  GdkWaylandSurface parent_instance;
};

typedef struct
{
  GdkWaylandSurfaceClass parent_class;
} GdkWaylandPopupClass;

static void gdk_wayland_popup_iface_init (GdkPopupInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GdkWaylandPopup, gdk_wayland_popup, GDK_TYPE_WAYLAND_SURFACE,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_POPUP,
                                                gdk_wayland_popup_iface_init))

typedef struct
{
  GdkWaylandSurface parent_instance;
} GdkWaylandDragSurface;

typedef struct
{
  GdkWaylandSurfaceClass parent_class;
} GdkWaylandDragSurfaceClass;

static void gdk_wayland_drag_surface_iface_init (GdkDragSurfaceInterface *iface);

GType gdk_wayland_drag_surface_get_type (void) G_GNUC_CONST;

#define GDK_IS_WAYLAND_DRAG_SURFACE(object) (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_WAYLAND_DRAG_SURFACE))

#define GDK_TYPE_WAYLAND_DRAG_SURFACE (gdk_wayland_drag_surface_get_type ())
G_DEFINE_TYPE_WITH_CODE (GdkWaylandDragSurface, gdk_wayland_drag_surface, GDK_TYPE_WAYLAND_SURFACE,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_DRAG_SURFACE,
                                                gdk_wayland_drag_surface_iface_init))

static void gdk_wayland_surface_maybe_resize (GdkSurface *surface,
                                              int         width,
                                              int         height,
                                              int         scale);

static void gdk_wayland_surface_configure (GdkSurface *surface);

static void maybe_set_gtk_surface_dbus_properties (GdkWaylandSurface *impl);
static void maybe_set_gtk_surface_modal (GdkSurface *surface);

static void gdk_wayland_surface_sync_shadow (GdkSurface *surface);
static void gdk_wayland_surface_sync_input_region (GdkSurface *surface);
static void gdk_wayland_surface_sync_opaque_region (GdkSurface *surface);

static void unset_transient_for_exported (GdkSurface *surface);

static void gdk_wayland_surface_move_resize (GdkSurface *surface,
                                             int         x,
                                             int         y,
                                             int         width,
                                             int         height);

static void update_popup_layout_state (GdkSurface     *surface,
                                       int             x,
                                       int             y,
                                       int             width,
                                       int             height,
                                       GdkPopupLayout *layout);

static gboolean gdk_wayland_toplevel_is_exported (GdkWaylandToplevel *wayland_toplevel);

static void configure_toplevel_geometry (GdkSurface *surface);

static void
gdk_wayland_surface_init (GdkWaylandSurface *impl)
{
  impl->scale = 1;
  impl->initial_fullscreen_output = NULL;
  impl->saved_width = -1;
  impl->saved_height = -1;
  impl->shortcuts_inhibitors = g_hash_table_new (NULL, NULL);
}

static void
gdk_wayland_surface_freeze_state (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  impl->state_freeze_count++;
}

static void
gdk_wayland_surface_thaw_state (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  g_assert (impl->state_freeze_count > 0);

  impl->state_freeze_count--;

  if (impl->state_freeze_count > 0)
    return;

  if (impl->pending.is_dirty)
    gdk_wayland_surface_configure (surface);

  g_assert (!impl->display_server.xdg_popup);
}

static void
_gdk_wayland_surface_save_size (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  if (surface->state & (GDK_TOPLEVEL_STATE_FULLSCREEN |
                        GDK_TOPLEVEL_STATE_MAXIMIZED |
                        GDK_TOPLEVEL_STATE_TILED))
    return;

  if (surface->width <= 1 || surface->height <= 1)
    return;

  impl->saved_width = surface->width - impl->shadow_left - impl->shadow_right;
  impl->saved_height = surface->height - impl->shadow_top - impl->shadow_bottom;
}

static void
_gdk_wayland_surface_clear_saved_size (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  if (surface->state & (GDK_TOPLEVEL_STATE_FULLSCREEN | GDK_TOPLEVEL_STATE_MAXIMIZED))
    return;

  impl->saved_width = -1;
  impl->saved_height = -1;
}

static void
gdk_wayland_surface_update_size (GdkSurface *surface,
                                 int32_t     width,
                                 int32_t     height,
                                 int         scale)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  gboolean width_changed, height_changed, scale_changed;

  width_changed = surface->width != width;
  height_changed = surface->height != height;
  scale_changed = impl->scale != scale;

  if (!width_changed && !height_changed && !scale_changed)
    return;

  surface->width = width;
  surface->height = height;
  impl->scale = scale;

  if (impl->display_server.egl_window)
    wl_egl_window_resize (impl->display_server.egl_window, width * scale, height * scale, 0, 0);
  if (impl->display_server.wl_surface)
    wl_surface_set_buffer_scale (impl->display_server.wl_surface, scale);

  gdk_surface_invalidate_rect (surface, NULL);

  if (width_changed)
    g_object_notify (G_OBJECT (surface), "width");
  if (height_changed)
    g_object_notify (G_OBJECT (surface), "height");
  if (scale_changed)
    g_object_notify (G_OBJECT (surface), "scale-factor");

  _gdk_surface_update_size (surface);
}

static const char *
get_default_title (void)
{
  const char *title;

  title = g_get_application_name ();
  if (!title)
    title = g_get_prgname ();
  if (!title)
    title = "";

  return title;
}

static void
fill_presentation_time_from_frame_time (GdkFrameTimings *timings,
                                        guint32          frame_time)
{
  /* The timestamp in a wayland frame is a msec time value that in some
   * way reflects the time at which the server started drawing the frame.
   * This is not useful from our perspective.
   *
   * However, for the DRM backend of Weston, on reasonably recent
   * Linux, we know that the time is the
   * clock_gettime (CLOCK_MONOTONIC) value at the vblank, and that
   * backend starts drawing immediately after receiving the vblank
   * notification. If we detect this, and make the assumption that the
   * compositor will finish drawing before the next vblank, we can
   * then determine the presentation time as the frame time we
   * received plus one refresh interval.
   *
   * If a backend is using clock_gettime(CLOCK_MONOTONIC), but not
   * picking values right at the vblank, then the presentation times
   * we compute won't be accurate, but not really worse than then
   * the alternative of not providing presentation times at all.
   *
   * The complexity here is dealing with the fact that we receive
   * only the low 32 bits of the CLOCK_MONOTONIC value in milliseconds.
   */
  gint64 now_monotonic = g_get_monotonic_time ();
  gint64 now_monotonic_msec = now_monotonic / 1000;
  uint32_t now_monotonic_low = (uint32_t)now_monotonic_msec;

  if (frame_time - now_monotonic_low < 1000 ||
      frame_time - now_monotonic_low > (uint32_t)-1000)
    {
      /* Timestamp we received is within one second of the current time.
       */
      gint64 last_frame_time = now_monotonic + (gint64)1000 * (gint32)(frame_time - now_monotonic_low);
      if ((gint32)now_monotonic_low < 0 && (gint32)frame_time > 0)
        last_frame_time += (gint64)1000 * G_GINT64_CONSTANT(0x100000000);
      else if ((gint32)now_monotonic_low > 0 && (gint32)frame_time < 0)
        last_frame_time -= (gint64)1000 * G_GINT64_CONSTANT(0x100000000);

      timings->presentation_time = last_frame_time + timings->refresh_interval;
    }
}

static GdkSurface *
get_popup_toplevel (GdkSurface *surface)
{
  if (surface->parent)
    return get_popup_toplevel (surface->parent);
  else
    return surface;
}

static void
freeze_popup_toplevel_state (GdkSurface *surface)
{
  GdkSurface *toplevel;

  toplevel = get_popup_toplevel (surface);
  gdk_wayland_surface_freeze_state (toplevel);
}

static void
thaw_popup_toplevel_state (GdkSurface *surface)
{
  GdkSurface *toplevel;

  toplevel = get_popup_toplevel (surface);
  gdk_wayland_surface_thaw_state (toplevel);
}

static void
finish_pending_relayout (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  g_assert (impl->popup_state == POPUP_STATE_WAITING_FOR_FRAME);
  impl->popup_state = POPUP_STATE_IDLE;

  thaw_popup_toplevel_state (surface);
}

static void
frame_callback (void               *data,
                struct wl_callback *callback,
                uint32_t            time)
{
  GdkSurface *surface = data;
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  GdkWaylandDisplay *display_wayland =
    GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));
  GdkFrameClock *clock = gdk_surface_get_frame_clock (surface);
  GdkFrameTimings *timings;

  gdk_profiler_add_mark (GDK_PROFILER_CURRENT_TIME, 0, "wayland", "frame event");
  GDK_DISPLAY_DEBUG (GDK_DISPLAY (display_wayland), EVENTS, "frame %p", surface);

  wl_callback_destroy (callback);

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  if (!impl->awaiting_frame)
    return;

  switch (impl->popup_state)
    {
    case POPUP_STATE_IDLE:
    case POPUP_STATE_WAITING_FOR_REPOSITIONED:
    case POPUP_STATE_WAITING_FOR_CONFIGURE:
      break;
    case POPUP_STATE_WAITING_FOR_FRAME:
      finish_pending_relayout (surface);
      break;
    default:
      g_assert_not_reached ();
    }

  impl->awaiting_frame = FALSE;
  if (impl->awaiting_frame_frozen)
    {
      impl->awaiting_frame_frozen = FALSE;
      gdk_surface_thaw_updates (surface);
    }

  timings = gdk_frame_clock_get_timings (clock, impl->pending_frame_counter);
  impl->pending_frame_counter = 0;

  if (timings == NULL)
    return;

  timings->refresh_interval = 16667; /* default to 1/60th of a second */
  if (impl->display_server.outputs)
    {
      /* We pick a random output out of the outputs that the surface touches
       * The rate here is in milli-hertz */
      int refresh_rate =
        gdk_wayland_display_get_output_refresh_rate (display_wayland,
                                                     impl->display_server.outputs->data);
      if (refresh_rate != 0)
        timings->refresh_interval = G_GINT64_CONSTANT(1000000000) / refresh_rate;
    }

  fill_presentation_time_from_frame_time (timings, time);

  timings->complete = TRUE;

#ifdef G_ENABLE_DEBUG
  if ((_gdk_debug_flags & GDK_DEBUG_FRAMES) != 0)
    _gdk_frame_clock_debug_print_timings (clock, timings);
#endif

  if (GDK_PROFILER_IS_RUNNING)
    _gdk_frame_clock_add_timings_to_profiler (clock, timings);
}

static const struct wl_callback_listener frame_listener = {
  frame_callback
};

static void
on_frame_clock_before_paint (GdkFrameClock *clock,
                             GdkSurface     *surface)
{
  GdkFrameTimings *timings = gdk_frame_clock_get_current_timings (clock);
  gint64 presentation_time;
  gint64 refresh_interval;

  if (surface->update_freeze_count > 0)
    return;

  gdk_frame_clock_get_refresh_info (clock,
                                    timings->frame_time,
                                    &refresh_interval, &presentation_time);

  if (presentation_time != 0)
    {
      /* Assume the algorithm used by the DRM backend of Weston - it
       * starts drawing at the next vblank after receiving the commit
       * for this frame, and presentation occurs at the vblank
       * after that.
       */
      timings->predicted_presentation_time = presentation_time + refresh_interval;
    }
  else
    {
      /* As above, but we don't actually know the phase of the vblank,
       * so just assume that we're half way through a refresh cycle.
       */
      timings->predicted_presentation_time = timings->frame_time + refresh_interval / 2 + refresh_interval;
    }

  gdk_surface_apply_state_change (surface);
}

static void
configure_popup_geometry (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  int x, y;
  int width, height;

  x = impl->next_layout.popup.x - impl->shadow_left;
  y = impl->next_layout.popup.y - impl->shadow_top;
  width =
    impl->next_layout.configured_width +
    (impl->shadow_left + impl->shadow_right);
  height =
    impl->next_layout.configured_height +
    (impl->shadow_top + impl->shadow_bottom);

  gdk_wayland_surface_move_resize (surface, x, y, width, height);
}

static void
configure_drag_surface_geometry (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  gdk_wayland_surface_update_size (surface,
                                   impl->next_layout.configured_width,
                                   impl->next_layout.configured_height,
                                   impl->scale);
}

static gboolean
gdk_wayland_surface_compute_size (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  if (impl->next_layout.surface_geometry_dirty)
    {
      if (GDK_IS_TOPLEVEL (impl))
        configure_toplevel_geometry (surface);
      else if (GDK_IS_POPUP (impl))
        configure_popup_geometry (surface);
      else if (GDK_IS_DRAG_SURFACE (impl))
        configure_drag_surface_geometry (surface);

      impl->next_layout.surface_geometry_dirty = FALSE;
    }

  return FALSE;
}

static void
gdk_wayland_surface_request_layout (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  impl->next_layout.surface_geometry_dirty = TRUE;
}

void
gdk_wayland_surface_request_frame (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  struct wl_callback *callback;
  GdkFrameClock *clock;

  if (impl->awaiting_frame)
    return;

  clock = gdk_surface_get_frame_clock (surface);

  callback = wl_surface_frame (impl->display_server.wl_surface);
  wl_proxy_set_queue ((struct wl_proxy *) callback, NULL);
  wl_callback_add_listener (callback, &frame_listener, surface);
  impl->pending_frame_counter = gdk_frame_clock_get_frame_counter (clock);
  impl->awaiting_frame = TRUE;
}

gboolean
gdk_wayland_surface_has_surface (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  return !!impl->display_server.wl_surface;
}

void
gdk_wayland_surface_commit (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  wl_surface_commit (impl->display_server.wl_surface);
}

void
gdk_wayland_surface_notify_committed (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  impl->has_uncommitted_ack_configure = FALSE;
}

static void
on_frame_clock_after_paint (GdkFrameClock *clock,
                            GdkSurface    *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  if (surface->update_freeze_count == 0 && impl->has_uncommitted_ack_configure)
    {
      gdk_wayland_surface_commit (surface);
      gdk_wayland_surface_notify_committed (surface);
    }

  if (impl->awaiting_frame &&
      impl->pending_frame_counter == gdk_frame_clock_get_frame_counter (clock))
    {
      impl->awaiting_frame_frozen = TRUE;
      gdk_surface_freeze_updates (surface);
    }
}

void
gdk_wayland_surface_update_scale (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));
  guint32 scale;
  GSList *l;

  if (display_wayland->compositor_version < WL_SURFACE_HAS_BUFFER_SCALE)
    {
      /* We can't set the scale on this surface */
      return;
    }

  if (!impl->display_server.outputs)
    {
      scale = impl->scale;
    }
  else
    {
      scale = 1;
      for (l = impl->display_server.outputs; l != NULL; l = l->next)
        {
          struct wl_output *output = l->data;
          uint32_t output_scale;

          output_scale = gdk_wayland_display_get_output_scale (display_wayland,
                                                               output);
          scale = MAX (scale, output_scale);
        }
    }

  /* Notify app that scale changed */
  gdk_wayland_surface_maybe_resize (surface,
                                    surface->width, surface->height,
                                    scale);
}

static void gdk_wayland_surface_create_surface (GdkSurface *surface);
static void gdk_wayland_surface_set_title      (GdkSurface *surface,
                                                const char *title);

GdkSurface *
_gdk_wayland_display_create_surface (GdkDisplay     *display,
                                     GdkSurfaceType  surface_type,
                                     GdkSurface     *parent,
                                     int             x,
                                     int             y,
                                     int             width,
                                     int             height)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);
  GdkSurface *surface;
  GdkWaylandSurface *impl;
  GdkFrameClock *frame_clock;

  if (parent)
    frame_clock = g_object_ref (gdk_surface_get_frame_clock (parent));
  else
    frame_clock = _gdk_frame_clock_idle_new ();

  switch (surface_type)
    {
    case GDK_SURFACE_TOPLEVEL:
      surface = g_object_new (GDK_TYPE_WAYLAND_TOPLEVEL,
                              "display", display,
                              "frame-clock", frame_clock,
                              NULL);
      display_wayland->toplevels = g_list_prepend (display_wayland->toplevels,
                                                   surface);
      g_warn_if_fail (!parent);
      break;
    case GDK_SURFACE_POPUP:
      surface = g_object_new (GDK_TYPE_WAYLAND_POPUP,
                              "parent", parent,
                              "display", display,
                              "frame-clock", frame_clock,
                              NULL);
      break;
    case GDK_SURFACE_TEMP:
      surface = g_object_new (GDK_TYPE_WAYLAND_DRAG_SURFACE,
                              "display", display,
                              "frame-clock", frame_clock,
                              NULL);
      break;
    default:
      g_assert_not_reached ();
      break;
    }

  impl = GDK_WAYLAND_SURFACE (surface);

  if (width > 65535)
    {
      g_warning ("Native Surfaces wider than 65535 pixels are not supported");
      width = 65535;
    }
  if (height > 65535)
    {
      g_warning ("Native Surfaces taller than 65535 pixels are not supported");
      height = 65535;
    }

  surface->x = x;
  surface->y = y;
  surface->width = width;
  surface->height = height;

  g_object_ref (surface);

  /* More likely to be right than just assuming 1 */
  if (display_wayland->compositor_version >= WL_SURFACE_HAS_BUFFER_SCALE)
    {
      GdkMonitor *monitor = g_list_model_get_item (gdk_display_get_monitors (display), 0);
      if (monitor)
        {
          impl->scale = gdk_monitor_get_scale_factor (monitor);
          g_object_unref (monitor);
        }
    }

  gdk_wayland_surface_set_title (surface, get_default_title ());


  gdk_wayland_surface_create_surface (surface);

  g_signal_connect (frame_clock, "before-paint", G_CALLBACK (on_frame_clock_before_paint), surface);
  g_signal_connect (frame_clock, "after-paint", G_CALLBACK (on_frame_clock_after_paint), surface);

  g_object_unref (frame_clock);

  return surface;
}

void
gdk_wayland_surface_attach_image (GdkSurface           *surface,
                                  cairo_surface_t      *cairo_surface,
                                  const cairo_region_t *damage)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  GdkWaylandDisplay *display;
  cairo_rectangle_int_t rect;
  int i, n;

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  g_assert (_gdk_wayland_is_shm_surface (cairo_surface));

  /* Attach this new buffer to the surface */
  wl_surface_attach (impl->display_server.wl_surface,
                     _gdk_wayland_shm_surface_get_wl_buffer (cairo_surface),
                     impl->pending_buffer_offset_x,
                     impl->pending_buffer_offset_y);
  impl->pending_buffer_offset_x = 0;
  impl->pending_buffer_offset_y = 0;

  /* Only set the buffer scale if supported by the compositor */
  display = GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));
  if (display->compositor_version >= WL_SURFACE_HAS_BUFFER_SCALE)
    wl_surface_set_buffer_scale (impl->display_server.wl_surface, impl->scale);

  n = cairo_region_num_rectangles (damage);
  for (i = 0; i < n; i++)
    {
      cairo_region_get_rectangle (damage, i, &rect);
      wl_surface_damage (impl->display_server.wl_surface, rect.x, rect.y, rect.width, rect.height);
    }
}

static void
gdk_wayland_surface_sync_offset (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  if (wl_surface_get_version (impl->display_server.wl_surface) <
      WL_SURFACE_OFFSET_SINCE_VERSION)
    return;

  if (impl->pending_buffer_offset_x == 0 &&
      impl->pending_buffer_offset_y == 0)
    return;

  wl_surface_offset (impl->display_server.wl_surface,
                     impl->pending_buffer_offset_x,
                     impl->pending_buffer_offset_y);
  impl->pending_buffer_offset_x = 0;
  impl->pending_buffer_offset_y = 0;
}

void
gdk_wayland_surface_sync (GdkSurface *surface)
{
  gdk_wayland_surface_sync_shadow (surface);
  gdk_wayland_surface_sync_opaque_region (surface);
  gdk_wayland_surface_sync_input_region (surface);
  gdk_wayland_surface_sync_offset (surface);
}

static gboolean
gdk_wayland_surface_beep (GdkSurface *surface)
{
  gdk_wayland_display_system_bell (gdk_surface_get_display (surface),
                                   surface);

  return TRUE;
}

static void
gdk_wayland_surface_constructed (GObject *object)
{
  GdkSurface *surface = GDK_SURFACE (object);
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  GdkWaylandDisplay *display_wayland =
    GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));

  G_OBJECT_CLASS (gdk_wayland_surface_parent_class)->constructed (object);

  impl->event_queue = wl_display_create_queue (display_wayland->wl_display);
  display_wayland->event_queues = g_list_prepend (display_wayland->event_queues,
                                                  impl->event_queue);
}

static void
gdk_wayland_surface_dispose (GObject *object)
{
  GdkSurface *surface = GDK_SURFACE (object);
  GdkWaylandSurface *impl;

  g_return_if_fail (GDK_IS_WAYLAND_SURFACE (surface));

  impl = GDK_WAYLAND_SURFACE (surface);

  if (impl->event_queue)
    {
      GdkWaylandDisplay *display_wayland =
        GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));

      display_wayland->event_queues =
        g_list_remove (display_wayland->event_queues, surface);
      g_clear_pointer (&impl->event_queue, wl_event_queue_destroy);
    }

  G_OBJECT_CLASS (gdk_wayland_surface_parent_class)->dispose (object);
}

static void
gdk_wayland_surface_finalize (GObject *object)
{
  GdkWaylandSurface *impl;

  g_return_if_fail (GDK_IS_WAYLAND_SURFACE (object));

  impl = GDK_WAYLAND_SURFACE (object);

  g_free (impl->title);

  g_free (impl->application.application_id);
  g_free (impl->application.app_menu_path);
  g_free (impl->application.menubar_path);
  g_free (impl->application.window_object_path);
  g_free (impl->application.application_object_path);
  g_free (impl->application.unique_bus_name);

  g_clear_pointer (&impl->opaque_region, cairo_region_destroy);
  g_clear_pointer (&impl->input_region, cairo_region_destroy);
  g_clear_pointer (&impl->shortcuts_inhibitors, g_hash_table_unref);

  G_OBJECT_CLASS (gdk_wayland_surface_parent_class)->finalize (object);
}

static gboolean
is_realized_shell_surface (GdkWaylandSurface *impl)
{
  return (impl->display_server.xdg_surface ||
          impl->display_server.zxdg_surface_v6);
}

static gboolean
is_realized_toplevel (GdkWaylandSurface *impl)
{
  return (impl->display_server.xdg_toplevel ||
          impl->display_server.zxdg_toplevel_v6);
}

static gboolean
is_realized_popup (GdkWaylandSurface *impl)
{
  return (impl->display_server.xdg_popup ||
          impl->display_server.zxdg_popup_v6);
}

static void gdk_wayland_surface_show (GdkSurface *surface);
static void gdk_wayland_surface_hide (GdkSurface *surface);

static void
gdk_wayland_surface_maybe_resize (GdkSurface *surface,
                                  int         width,
                                  int         height,
                                  int         scale)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  gboolean is_xdg_popup;
  gboolean is_visible;

  if (surface->width == width &&
      surface->height == height &&
      impl->scale == scale)
    return;

  /* For xdg_popup using an xdg_positioner, there is a race condition if
   * the application tries to change the size after it's mapped, but before
   * the initial configure is received, so hide and show the surface again
   * force the new size onto the compositor. See bug #772505.
   */

  is_xdg_popup = is_realized_popup (impl);
  is_visible = gdk_surface_get_mapped (surface);

  if (is_xdg_popup && is_visible && !impl->initial_configure_received)
    gdk_wayland_surface_hide (surface);

  gdk_wayland_surface_update_size (surface, width, height, scale);

  if (is_xdg_popup && is_visible && !impl->initial_configure_received)
    gdk_wayland_surface_show (surface);
}

static void
gdk_wayland_surface_sync_parent (GdkSurface *surface,
                                 GdkSurface *parent)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  GdkWaylandToplevel *toplevel = GDK_WAYLAND_TOPLEVEL (impl);
  GdkWaylandDisplay *display_wayland =
    GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));
  GdkWaylandSurface *impl_parent = NULL;

  g_assert (parent == NULL ||
            gdk_surface_get_display (surface) == gdk_surface_get_display (parent));

  if (!is_realized_toplevel (impl))
    return;

  if (toplevel->transient_for)
    impl_parent = GDK_WAYLAND_SURFACE (toplevel->transient_for);
  else if (parent)
    impl_parent = GDK_WAYLAND_SURFACE (parent);

  /* XXX: Is this correct? */
  if (impl_parent && !impl_parent->display_server.wl_surface)
    return;

  switch (display_wayland->shell_variant)
    {
    case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
      {
        struct xdg_toplevel *parent_toplevel;

        if (impl_parent)
          parent_toplevel = impl_parent->display_server.xdg_toplevel;
        else
          parent_toplevel = NULL;

        xdg_toplevel_set_parent (impl->display_server.xdg_toplevel,
                                 parent_toplevel);
        break;
      }
      break;
    case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
      {
        struct zxdg_toplevel_v6 *parent_toplevel;

        if (impl_parent)
          parent_toplevel = impl_parent->display_server.zxdg_toplevel_v6;
        else
          parent_toplevel = NULL;

        zxdg_toplevel_v6_set_parent (impl->display_server.zxdg_toplevel_v6,
                                     parent_toplevel);
        break;
      }
    default:
      g_assert_not_reached ();
    }
}

static void
gdk_wayland_surface_sync_parent_of_imported (GdkWaylandSurface *impl)
{
  if (!impl->display_server.wl_surface)
    return;

  if (!impl->imported_transient_for)
    return;

  if (!is_realized_toplevel (impl))
    return;

  zxdg_imported_v1_set_parent_of (impl->imported_transient_for,
                                  impl->display_server.wl_surface);
}

static void
gdk_wayland_surface_sync_title (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  GdkWaylandDisplay *display_wayland =
    GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));

  if (!is_realized_toplevel (impl))
    return;

  if (!impl->title)
    return;

  switch (display_wayland->shell_variant)
    {
    case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
      xdg_toplevel_set_title (impl->display_server.xdg_toplevel,
                              impl->title);
      break;
    case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
      zxdg_toplevel_v6_set_title (impl->display_server.zxdg_toplevel_v6,
                                  impl->title);
      break;
    default:
      g_assert_not_reached ();
    }
}

static void
gdk_wayland_surface_get_window_geometry (GdkSurface   *surface,
                                         GdkRectangle *geometry)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  *geometry = (GdkRectangle) {
    .x = impl->shadow_left,
    .y = impl->shadow_top,
    .width = surface->width - (impl->shadow_left + impl->shadow_right),
    .height = surface->height - (impl->shadow_top + impl->shadow_bottom)
  };
}

static void gdk_wayland_surface_set_geometry_hints (GdkWaylandSurface  *impl,
                                                    const GdkGeometry  *geometry,
                                                    GdkSurfaceHints     geom_mask);

static void
gdk_wayland_surface_sync_shadow (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  GdkWaylandDisplay *display_wayland =
    GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));
  GdkRectangle geometry;

  if (!is_realized_shell_surface (impl))
    return;

  gdk_wayland_surface_get_window_geometry (surface, &geometry);
  gdk_wayland_surface_set_geometry_hints (impl,
                                          &impl->geometry_hints,
                                          impl->geometry_mask);

  if (gdk_rectangle_equal (&geometry, &impl->last_sent_window_geometry))
    return;

  switch (display_wayland->shell_variant)
    {
    case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
      xdg_surface_set_window_geometry (impl->display_server.xdg_surface,
                                       geometry.x,
                                       geometry.y,
                                       geometry.width,
                                       geometry.height);
      break;
    case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
      zxdg_surface_v6_set_window_geometry (impl->display_server.zxdg_surface_v6,
                                           geometry.x,
                                           geometry.y,
                                           geometry.width,
                                           geometry.height);
      break;
    default:
      g_assert_not_reached ();
    }

  impl->last_sent_window_geometry = geometry;
}

static struct wl_region *
wl_region_from_cairo_region (GdkWaylandDisplay *display,
                             cairo_region_t    *region)
{
  struct wl_region *wl_region;
  int i, n_rects;

  wl_region = wl_compositor_create_region (display->compositor);
  if (wl_region == NULL)
    return NULL;

  n_rects = cairo_region_num_rectangles (region);
  for (i = 0; i < n_rects; i++)
    {
      cairo_rectangle_int_t rect;
      cairo_region_get_rectangle (region, i, &rect);
      wl_region_add (wl_region, rect.x, rect.y, rect.width, rect.height);
    }

  return wl_region;
}

static void
gdk_wayland_surface_sync_opaque_region (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  struct wl_region *wl_region = NULL;

  if (!impl->display_server.wl_surface)
    return;

  if (!impl->opaque_region_dirty)
    return;

  if (impl->opaque_region != NULL)
    wl_region = wl_region_from_cairo_region (GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface)),
                                             impl->opaque_region);

  wl_surface_set_opaque_region (impl->display_server.wl_surface, wl_region);

  if (wl_region != NULL)
    wl_region_destroy (wl_region);

  impl->opaque_region_dirty = FALSE;
}

static void
gdk_wayland_surface_sync_input_region (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  struct wl_region *wl_region = NULL;

  if (!impl->display_server.wl_surface)
    return;

  if (!impl->input_region_dirty)
    return;

  if (impl->input_region != NULL)
    wl_region = wl_region_from_cairo_region (GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface)),
                                             impl->input_region);

  wl_surface_set_input_region (impl->display_server.wl_surface, wl_region);

  if (wl_region != NULL)
    wl_region_destroy (wl_region);

  impl->input_region_dirty = FALSE;
}

static void
surface_enter (void              *data,
               struct wl_surface *wl_surface,
               struct wl_output  *output)
{
  GdkSurface *surface = GDK_SURFACE (data);
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  GdkDisplay *display = gdk_surface_get_display (surface);
  GdkMonitor *monitor;

  GDK_DISPLAY_DEBUG(gdk_surface_get_display (surface), EVENTS,
                    "surface enter, surface %p output %p", surface, output);

  impl->display_server.outputs = g_slist_prepend (impl->display_server.outputs, output);

  gdk_wayland_surface_update_scale (surface);

  monitor = gdk_wayland_display_get_monitor_for_output (display, output);
  gdk_surface_enter_monitor (surface, monitor);
}

static void
surface_leave (void              *data,
               struct wl_surface *wl_surface,
               struct wl_output  *output)
{
  GdkSurface *surface = GDK_SURFACE (data);
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  GdkDisplay *display = gdk_surface_get_display (surface);
  GdkMonitor *monitor;

  GDK_DISPLAY_DEBUG (gdk_surface_get_display (surface), EVENTS,
                     "surface leave, surface %p output %p", surface, output);

  impl->display_server.outputs = g_slist_remove (impl->display_server.outputs, output);

  if (impl->display_server.outputs)
    gdk_wayland_surface_update_scale (surface);

  monitor = gdk_wayland_display_get_monitor_for_output (display, output);
  gdk_surface_leave_monitor (surface, monitor);
}

static const struct wl_surface_listener surface_listener = {
  surface_enter,
  surface_leave
};

static void
gdk_wayland_surface_create_surface (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));
  struct wl_surface *wl_surface;

  wl_surface = wl_compositor_create_surface (display_wayland->compositor);
  wl_proxy_set_queue ((struct wl_proxy *) wl_surface, impl->event_queue);
  wl_surface_add_listener (wl_surface, &surface_listener, surface);

  impl->display_server.wl_surface = wl_surface;
}

static void
configure_toplevel_geometry (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  GdkDisplay *display = gdk_surface_get_display (surface);
  int bounds_width, bounds_height;
  GdkToplevelSize size;
  GdkToplevelLayout *layout;
  GdkGeometry geometry;
  GdkSurfaceHints mask;

  if (impl->toplevel.has_bounds)
    {
      bounds_width = impl->toplevel.bounds_width;
      bounds_height = impl->toplevel.bounds_height;
    }
  else
    {
      GdkMonitor *monitor;
      GListModel *monitors;
      GdkRectangle monitor_geometry, display_geometry = { 0 };
      guint i;

      monitors = gdk_display_get_monitors (display);

      for (i = 0; i < g_list_model_get_n_items (monitors); i++)
        {
          monitor = g_list_model_get_item (monitors, i);
          gdk_monitor_get_geometry (monitor, &monitor_geometry);
          gdk_rectangle_union (&display_geometry, &monitor_geometry, &display_geometry);
          g_object_unref (monitor);
        }

      bounds_width = display_geometry.width;
      bounds_height = display_geometry.height;
    }

  gdk_toplevel_size_init (&size, bounds_width, bounds_height);
  gdk_toplevel_notify_compute_size (GDK_TOPLEVEL (surface), &size);
  g_warn_if_fail (size.width > 0);
  g_warn_if_fail (size.height > 0);

  layout = impl->toplevel.layout;
  if (gdk_toplevel_layout_get_resizable (layout))
    {
      geometry.min_width = size.min_width;
      geometry.min_height = size.min_height;
      mask = GDK_HINT_MIN_SIZE;
    }
  else
    {
      geometry.max_width = geometry.min_width = size.width;
      geometry.max_height = geometry.min_height = size.height;
      mask = GDK_HINT_MIN_SIZE | GDK_HINT_MAX_SIZE;
    }
  gdk_wayland_surface_set_geometry_hints (impl, &geometry, mask);

  if (size.shadow.is_valid)
    {
      impl->shadow_left = size.shadow.left;
      impl->shadow_right = size.shadow.right;
      impl->shadow_top = size.shadow.top;
      impl->shadow_bottom = size.shadow.bottom;
    }

  if (impl->next_layout.configured_width > 0 &&
      impl->next_layout.configured_height > 0)
    {
      int width, height;

      width = impl->next_layout.configured_width +
        impl->shadow_left + impl->shadow_right;
      height = impl->next_layout.configured_height +
        impl->shadow_top + impl->shadow_bottom;

      if (impl->next_layout.toplevel.should_constrain)
        {
          gdk_surface_constrain_size (&impl->geometry_hints,
                                      impl->geometry_mask,
                                      width, height,
                                      &width, &height);
        }
      gdk_wayland_surface_update_size (surface, width, height, impl->scale);

      if (!impl->next_layout.toplevel.size_is_fixed)
        {
          impl->next_layout.toplevel.should_constrain = FALSE;
          impl->next_layout.configured_width = 0;
          impl->next_layout.configured_height = 0;
        }
    }
  else
    {
      int width, height;

      width = size.width;
      height = size.height;
      gdk_surface_constrain_size (&geometry, mask,
                                  width, height,
                                  &width, &height);
      gdk_wayland_surface_update_size (surface, width, height, impl->scale);
    }
}

static void
synthesize_initial_surface_state (GdkSurface       *surface,
                                  GdkToplevelState  unset_flags,
                                  GdkToplevelState  set_flags)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  impl->initial_state.unset_flags |= unset_flags;
  impl->initial_state.set_flags &= ~unset_flags;

  impl->initial_state.set_flags |= set_flags;
  impl->initial_state.unset_flags &= ~set_flags;
}

static void
gdk_wayland_surface_configure_toplevel (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  GdkWaylandDisplay *display_wayland =
    GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));
  GdkToplevelState new_state;
  int width, height;
  gboolean is_resizing;
  gboolean fixed_size;
  gboolean was_fixed_size;
  gboolean saved_size;

  new_state = impl->pending.toplevel.state;
  impl->pending.toplevel.state = 0;

  is_resizing = impl->pending.toplevel.is_resizing;
  impl->pending.toplevel.is_resizing = FALSE;

  if (impl->pending.toplevel.has_bounds)
    {
      impl->toplevel.bounds_width = impl->pending.toplevel.bounds_width;
      impl->toplevel.bounds_height = impl->pending.toplevel.bounds_height;
      impl->toplevel.has_bounds = TRUE;
    }

  fixed_size =
    new_state & (GDK_TOPLEVEL_STATE_MAXIMIZED |
                 GDK_TOPLEVEL_STATE_FULLSCREEN |
                 GDK_TOPLEVEL_STATE_TILED) ||
    is_resizing;

  was_fixed_size =
    surface->state & (GDK_TOPLEVEL_STATE_MAXIMIZED |
                      GDK_TOPLEVEL_STATE_FULLSCREEN |
                      GDK_TOPLEVEL_STATE_TILED);

  width = impl->pending.toplevel.width;
  height = impl->pending.toplevel.height;

  saved_size = (width == 0 && height == 0);
  /* According to xdg_shell, an xdg_surface.configure with size 0x0
   * should be interpreted as that it is up to the client to set a
   * size.
   *
   * When transitioning from maximize or fullscreen state, this means
   * the client should configure its size back to what it was before
   * being maximize or fullscreen.
   */
  if (saved_size && !fixed_size && was_fixed_size)
    {
      width = impl->saved_width;
      height = impl->saved_height;
    }

  if (width > 0 && height > 0)
    {
      if (!saved_size)
        {
          impl->next_layout.toplevel.should_constrain = TRUE;

          /* Save size for next time we get 0x0 */
          _gdk_wayland_surface_save_size (surface);
        }
      else if (is_resizing)
        {
          impl->next_layout.toplevel.should_constrain = TRUE;
        }
      else
        {
          impl->next_layout.toplevel.should_constrain = FALSE;
        }

      impl->next_layout.toplevel.size_is_fixed = fixed_size;
      impl->next_layout.configured_width = width;
      impl->next_layout.configured_height = height;
    }
  else
    {
      impl->next_layout.toplevel.should_constrain = FALSE;
      impl->next_layout.toplevel.size_is_fixed = FALSE;
      impl->next_layout.configured_width = 0;
      impl->next_layout.configured_height = 0;
    }

  impl->next_layout.surface_geometry_dirty = TRUE;
  gdk_surface_request_layout (surface);

  GDK_DISPLAY_DEBUG (gdk_surface_get_display (surface), EVENTS,
                     "configure, surface %p %dx%d,%s%s%s%s",
                     surface, width, height,
                     (new_state & GDK_TOPLEVEL_STATE_FULLSCREEN) ? " fullscreen" : "",
                     (new_state & GDK_TOPLEVEL_STATE_MAXIMIZED) ? " maximized" : "",
                     (new_state & GDK_TOPLEVEL_STATE_FOCUSED) ? " focused" : "",
                     (new_state & GDK_TOPLEVEL_STATE_TILED) ? " tiled" : "");

  gdk_surface_queue_state_change (surface, ~0 & ~new_state, new_state);

  switch (display_wayland->shell_variant)
    {
    case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
      xdg_surface_ack_configure (impl->display_server.xdg_surface,
                                 impl->pending.serial);
      break;
    case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
      zxdg_surface_v6_ack_configure (impl->display_server.zxdg_surface_v6,
                                     impl->pending.serial);
      break;
    default:
      g_assert_not_reached ();
    }
}

static void
gdk_wayland_surface_configure_popup (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  GdkRectangle parent_geometry;
  int x, y, width, height;

  if (impl->display_server.xdg_popup)
    {
      xdg_surface_ack_configure (impl->display_server.xdg_surface,
                                 impl->pending.serial);
    }
  else if (impl->display_server.zxdg_popup_v6)
    {
      zxdg_surface_v6_ack_configure (impl->display_server.zxdg_surface_v6,
                                     impl->pending.serial);
    }

  if (impl->pending.popup.has_repositioned_token)
    impl->received_reposition_token = impl->pending.popup.repositioned_token;

  switch (impl->popup_state)
    {
    case POPUP_STATE_WAITING_FOR_REPOSITIONED:
      if (impl->received_reposition_token != impl->reposition_token)
        return;
      else
        gdk_surface_thaw_updates (surface);
      G_GNUC_FALLTHROUGH;
    case POPUP_STATE_WAITING_FOR_CONFIGURE:
      impl->popup_state = POPUP_STATE_WAITING_FOR_FRAME;
      break;
    case POPUP_STATE_IDLE:
    case POPUP_STATE_WAITING_FOR_FRAME:
      break;
    default:
      g_assert_not_reached ();
    }

  x = impl->pending.popup.x;
  y = impl->pending.popup.y;
  width = impl->pending.popup.width;
  height = impl->pending.popup.height;

  gdk_wayland_surface_get_window_geometry (surface->parent, &parent_geometry);
  x += parent_geometry.x;
  y += parent_geometry.y;

  update_popup_layout_state (surface,
                             x, y,
                             width, height,
                             impl->popup.layout);

  impl->next_layout.popup.x = x;
  impl->next_layout.popup.y = y;
  impl->next_layout.configured_width = width;
  impl->next_layout.configured_height = height;
  impl->next_layout.surface_geometry_dirty = TRUE;
  gdk_surface_request_layout (surface);
}

static void
maybe_notify_mapped (GdkSurface *surface)
{
  if (surface->destroyed)
    return;

  if (!GDK_SURFACE_IS_MAPPED (surface))
    gdk_surface_set_is_mapped (surface, TRUE);
}

static void
gdk_wayland_surface_configure (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  if (!impl->initial_configure_received)
    {
      gdk_surface_thaw_updates (surface);
      impl->initial_configure_received = TRUE;
      impl->pending.is_initial_configure = TRUE;
      maybe_notify_mapped (surface);
    }

  impl->has_uncommitted_ack_configure = TRUE;

  if (is_realized_popup (impl))
    gdk_wayland_surface_configure_popup (surface);
  else if (is_realized_toplevel (impl))
    gdk_wayland_surface_configure_toplevel (surface);
  else
    g_warn_if_reached ();

  impl->last_configure_serial = impl->pending.serial;

  memset (&impl->pending, 0, sizeof (impl->pending));
}

static void
gdk_wayland_surface_handle_configure (GdkSurface *surface,
                                      uint32_t    serial)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  impl->pending.is_dirty = TRUE;
  impl->pending.serial = serial;

  if (impl->state_freeze_count > 0)
    return;

  gdk_wayland_surface_configure (surface);
}

static void
gdk_wayland_surface_handle_configure_toplevel (GdkSurface      *surface,
                                               int32_t          width,
                                               int32_t          height,
                                               GdkToplevelState  state)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  impl->pending.toplevel.state |= state;
  impl->pending.toplevel.width = width;
  impl->pending.toplevel.height = height;
}

static void
gdk_wayland_surface_handle_close (GdkSurface *surface)
{
  GdkDisplay *display;
  GdkEvent *event;

  display = gdk_surface_get_display (surface);

  GDK_DISPLAY_DEBUG (display, EVENTS, "close %p", surface);

  event = gdk_delete_event_new (surface);

  _gdk_wayland_display_deliver_event (display, event);
}

static void
xdg_surface_configure (void               *data,
                       struct xdg_surface *xdg_surface,
                       uint32_t            serial)
{
  GdkSurface *surface = GDK_SURFACE (data);

  gdk_wayland_surface_handle_configure (surface, serial);
}

static const struct xdg_surface_listener xdg_surface_listener = {
  xdg_surface_configure,
};

static void
zxdg_surface_v6_configure (void                   *data,
                           struct zxdg_surface_v6 *xdg_surface,
                           uint32_t                serial)
{
  GdkSurface *surface = GDK_SURFACE (data);

  gdk_wayland_surface_handle_configure (surface, serial);
}

static const struct zxdg_surface_v6_listener zxdg_surface_v6_listener = {
  zxdg_surface_v6_configure,
};

static void
gdk_wayland_surface_create_xdg_surface_resources (GdkSurface *surface)
{
  GdkWaylandDisplay *display =
    GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  switch (display->shell_variant)
    {
    case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
      impl->display_server.xdg_surface =
        xdg_wm_base_get_xdg_surface (display->xdg_wm_base,
                                     impl->display_server.wl_surface);
      wl_proxy_set_queue ((struct wl_proxy *) impl->display_server.xdg_surface,
                          impl->event_queue);
      xdg_surface_add_listener (impl->display_server.xdg_surface,
                                &xdg_surface_listener,
                                surface);
      break;
    case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
      impl->display_server.zxdg_surface_v6 =
        zxdg_shell_v6_get_xdg_surface (display->zxdg_shell_v6,
                                       impl->display_server.wl_surface);
      zxdg_surface_v6_add_listener (impl->display_server.zxdg_surface_v6,
                                    &zxdg_surface_v6_listener,
                                    surface);
      break;
    default:
      g_assert_not_reached ();
    }
}

static void
xdg_toplevel_configure (void                *data,
                        struct xdg_toplevel *xdg_toplevel,
                        int32_t              width,
                        int32_t              height,
                        struct wl_array     *states)
{
  GdkSurface *surface = GDK_SURFACE (data);
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  uint32_t *p;
  GdkToplevelState pending_state = 0;

  impl->pending.toplevel.is_resizing = FALSE;

  wl_array_for_each (p, states)
    {
      uint32_t state = *p;

      switch (state)
        {
        case XDG_TOPLEVEL_STATE_FULLSCREEN:
          pending_state |= GDK_TOPLEVEL_STATE_FULLSCREEN;
          break;
        case XDG_TOPLEVEL_STATE_MAXIMIZED:
          pending_state |= GDK_TOPLEVEL_STATE_MAXIMIZED;
          break;
        case XDG_TOPLEVEL_STATE_ACTIVATED:
          pending_state |= GDK_TOPLEVEL_STATE_FOCUSED;
          break;
        case XDG_TOPLEVEL_STATE_RESIZING:
          impl->pending.toplevel.is_resizing = TRUE;
          break;
        default:
          /* Unknown state */
          break;
        }
    }

  gdk_wayland_surface_handle_configure_toplevel (surface, width, height,
                                                 pending_state);
}

static void
xdg_toplevel_close (void                *data,
                    struct xdg_toplevel *xdg_toplevel)
{
  GdkSurface *surface = GDK_SURFACE (data);

  gdk_wayland_surface_handle_close (surface);
}

static void
xdg_toplevel_configure_bounds (void                *data,
                               struct xdg_toplevel *xdg_toplevel,
                               int32_t              width,
                               int32_t              height)
{
  GdkSurface *surface = GDK_SURFACE (data);
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  impl->pending.toplevel.bounds_width = width;
  impl->pending.toplevel.bounds_height = height;
  impl->pending.toplevel.has_bounds = TRUE;
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
  xdg_toplevel_configure,
  xdg_toplevel_close,
  xdg_toplevel_configure_bounds,
};

static void
create_xdg_toplevel_resources (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  impl->display_server.xdg_toplevel =
    xdg_surface_get_toplevel (impl->display_server.xdg_surface);
  xdg_toplevel_add_listener (impl->display_server.xdg_toplevel,
                             &xdg_toplevel_listener,
                             surface);
}

static void
zxdg_toplevel_v6_configure (void                    *data,
                            struct zxdg_toplevel_v6 *xdg_toplevel,
                            int32_t                  width,
                            int32_t                  height,
                            struct wl_array         *states)
{
  GdkSurface *surface = GDK_SURFACE (data);
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  uint32_t *p;
  GdkToplevelState pending_state = 0;

  impl->pending.toplevel.is_resizing = FALSE;

  wl_array_for_each (p, states)
    {
      uint32_t state = *p;

      switch (state)
        {
        case ZXDG_TOPLEVEL_V6_STATE_FULLSCREEN:
          pending_state |= GDK_TOPLEVEL_STATE_FULLSCREEN;
          break;
        case ZXDG_TOPLEVEL_V6_STATE_MAXIMIZED:
          pending_state |= GDK_TOPLEVEL_STATE_MAXIMIZED;
          break;
        case ZXDG_TOPLEVEL_V6_STATE_ACTIVATED:
          pending_state |= GDK_TOPLEVEL_STATE_FOCUSED;
          break;
        case ZXDG_TOPLEVEL_V6_STATE_RESIZING:
          impl->pending.toplevel.is_resizing = TRUE;
          break;
        default:
          /* Unknown state */
          break;
        }
    }

  gdk_wayland_surface_handle_configure_toplevel (surface, width, height,
                                                 pending_state);
}

static void
zxdg_toplevel_v6_close (void                    *data,
                        struct zxdg_toplevel_v6 *xdg_toplevel)
{
  GdkSurface *surface = GDK_SURFACE (data);

  gdk_wayland_surface_handle_close (surface);
}

static const struct zxdg_toplevel_v6_listener zxdg_toplevel_v6_listener = {
  zxdg_toplevel_v6_configure,
  zxdg_toplevel_v6_close,
};

static void
create_zxdg_toplevel_v6_resources (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  impl->display_server.zxdg_toplevel_v6 =
    zxdg_surface_v6_get_toplevel (impl->display_server.zxdg_surface_v6);
  zxdg_toplevel_v6_add_listener (impl->display_server.zxdg_toplevel_v6,
                                 &zxdg_toplevel_v6_listener,
                                 surface);
}

/**
 * gdk_wayland_toplevel_set_application_id:
 * @toplevel: (type GdkWaylandToplevel): a `GdkToplevel`
 * @application_id: the application id for the @toplevel
 *
 * Sets the application id on a `GdkToplevel`.
 */
void
gdk_wayland_toplevel_set_application_id (GdkToplevel *toplevel,
                                         const char  *application_id)
{
  GdkWaylandSurface *impl;
  GdkWaylandDisplay *display_wayland;

  g_return_if_fail (GDK_IS_WAYLAND_TOPLEVEL (toplevel));

  g_return_if_fail (application_id != NULL);

  if (GDK_SURFACE_DESTROYED (toplevel))
    return;

  impl = GDK_WAYLAND_SURFACE (toplevel);

  if (!is_realized_toplevel (impl))
    return;

  display_wayland = GDK_WAYLAND_DISPLAY (gdk_surface_get_display (GDK_SURFACE (toplevel)));

  switch (display_wayland->shell_variant)
    {
    case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
      xdg_toplevel_set_app_id (impl->display_server.xdg_toplevel,
                               application_id);
      break;
    case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
      zxdg_toplevel_v6_set_app_id (impl->display_server.zxdg_toplevel_v6,
                                   application_id);
      break;
    default:
      g_assert_not_reached ();
    }
}

static void
gdk_wayland_surface_create_xdg_toplevel (GdkSurface *surface)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  const char *app_id;

  gdk_surface_freeze_updates (surface);
  gdk_wayland_surface_create_xdg_surface_resources (surface);

  switch (display_wayland->shell_variant)
    {
    case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
      create_xdg_toplevel_resources (surface);
      break;
    case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
      create_zxdg_toplevel_v6_resources (surface);
      break;
    default:
      g_assert_not_reached ();
    }

  gdk_wayland_surface_sync_parent (surface, NULL);
  gdk_wayland_surface_sync_parent_of_imported (impl);
  gdk_wayland_surface_sync_title (surface);

  switch (display_wayland->shell_variant)
    {
    case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
      if (impl->initial_state.set_flags & GDK_TOPLEVEL_STATE_MAXIMIZED)
        xdg_toplevel_set_maximized (impl->display_server.xdg_toplevel);
      if (impl->initial_state.set_flags & GDK_TOPLEVEL_STATE_MINIMIZED)
        xdg_toplevel_set_minimized (impl->display_server.xdg_toplevel);
      if (impl->initial_state.set_flags & GDK_TOPLEVEL_STATE_FULLSCREEN)
        xdg_toplevel_set_fullscreen (impl->display_server.xdg_toplevel,
                                     impl->initial_fullscreen_output);
      break;
    case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
      if (impl->initial_state.set_flags & GDK_TOPLEVEL_STATE_MAXIMIZED)
        zxdg_toplevel_v6_set_maximized (impl->display_server.zxdg_toplevel_v6);
      if (impl->initial_state.set_flags & GDK_TOPLEVEL_STATE_MINIMIZED)
        zxdg_toplevel_v6_set_minimized (impl->display_server.zxdg_toplevel_v6);
      if (impl->initial_state.set_flags & GDK_TOPLEVEL_STATE_FULLSCREEN)
        zxdg_toplevel_v6_set_fullscreen (impl->display_server.zxdg_toplevel_v6,
                                         impl->initial_fullscreen_output);
      break;
    default:
      g_assert_not_reached ();
    }

  impl->initial_fullscreen_output = NULL;

  app_id = impl->application.application_id;
  if (app_id == NULL)
    app_id = g_get_prgname ();

  if (app_id == NULL)
    app_id = "GTK Application";

  gdk_wayland_toplevel_set_application_id (GDK_TOPLEVEL (impl), app_id);

  maybe_set_gtk_surface_dbus_properties (impl);
  maybe_set_gtk_surface_modal (surface);

  gdk_profiler_add_mark (GDK_PROFILER_CURRENT_TIME, 0, "wayland", "surface commit");
  wl_surface_commit (impl->display_server.wl_surface);
}

static void
gdk_wayland_surface_handle_configure_popup (GdkSurface *surface,
                                            int32_t     x,
                                            int32_t     y,
                                            int32_t     width,
                                            int32_t     height)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  impl->pending.popup.x = x;
  impl->pending.popup.y = y;
  impl->pending.popup.width = width;
  impl->pending.popup.height = height;
}

static void
xdg_popup_configure (void             *data,
                     struct xdg_popup *xdg_popup,
                     int32_t           x,
                     int32_t           y,
                     int32_t           width,
                     int32_t           height)
{
  GdkSurface *surface = GDK_SURFACE (data);

  gdk_wayland_surface_handle_configure_popup (surface, x, y, width, height);
}

static void
xdg_popup_done (void             *data,
                struct xdg_popup *xdg_popup)
{
  GdkSurface *surface = GDK_SURFACE (data);

  GDK_DISPLAY_DEBUG (gdk_surface_get_display (surface), EVENTS, "done %p", surface);

  gdk_surface_hide (surface);
}

static void
xdg_popup_repositioned (void             *data,
                        struct xdg_popup *xdg_popup,
                        uint32_t          token)
{
  GdkSurface *surface = GDK_SURFACE (data);
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  GDK_DISPLAY_DEBUG (gdk_surface_get_display (surface), EVENTS,
                     "repositioned %p", surface);

  if (impl->popup_state != POPUP_STATE_WAITING_FOR_REPOSITIONED)
    {
      g_warning ("Unexpected xdg_popup.repositioned event, probably buggy compositor");
      return;
    }

  impl->pending.popup.repositioned_token = token;
  impl->pending.popup.has_repositioned_token = TRUE;
}

static const struct xdg_popup_listener xdg_popup_listener = {
  xdg_popup_configure,
  xdg_popup_done,
  xdg_popup_repositioned,
};

static void
zxdg_popup_v6_configure (void                 *data,
                         struct zxdg_popup_v6 *xdg_popup,
                         int32_t               x,
                         int32_t               y,
                         int32_t               width,
                         int32_t               height)
{
  GdkSurface *surface = GDK_SURFACE (data);

  gdk_wayland_surface_handle_configure_popup (surface, x, y, width, height);
}

static void
zxdg_popup_v6_done (void                 *data,
                    struct zxdg_popup_v6 *xdg_popup)
{
  GdkSurface *surface = GDK_SURFACE (data);

  GDK_DEBUG (EVENTS, "done %p", surface);

  gdk_surface_hide (surface);
}

static const struct zxdg_popup_v6_listener zxdg_popup_v6_listener = {
  zxdg_popup_v6_configure,
  zxdg_popup_v6_done,
};

static enum xdg_positioner_anchor
rect_anchor_to_anchor (GdkGravity rect_anchor)
{
  switch (rect_anchor)
    {
    case GDK_GRAVITY_NORTH_WEST:
    case GDK_GRAVITY_STATIC:
      return XDG_POSITIONER_ANCHOR_TOP_LEFT;
    case GDK_GRAVITY_NORTH:
      return XDG_POSITIONER_ANCHOR_TOP;
    case GDK_GRAVITY_NORTH_EAST:
      return XDG_POSITIONER_ANCHOR_TOP_RIGHT;
    case GDK_GRAVITY_WEST:
      return XDG_POSITIONER_ANCHOR_LEFT;
    case GDK_GRAVITY_CENTER:
      return XDG_POSITIONER_ANCHOR_NONE;
    case GDK_GRAVITY_EAST:
      return XDG_POSITIONER_ANCHOR_RIGHT;
    case GDK_GRAVITY_SOUTH_WEST:
      return XDG_POSITIONER_ANCHOR_BOTTOM_LEFT;
    case GDK_GRAVITY_SOUTH:
      return XDG_POSITIONER_ANCHOR_BOTTOM;
    case GDK_GRAVITY_SOUTH_EAST:
      return XDG_POSITIONER_ANCHOR_BOTTOM_RIGHT;
    default:
      g_assert_not_reached ();
    }
}

static enum xdg_positioner_gravity
surface_anchor_to_gravity (GdkGravity rect_anchor)
{
  switch (rect_anchor)
    {
    case GDK_GRAVITY_NORTH_WEST:
    case GDK_GRAVITY_STATIC:
      return XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT;
    case GDK_GRAVITY_NORTH:
      return XDG_POSITIONER_GRAVITY_BOTTOM;
    case GDK_GRAVITY_NORTH_EAST:
      return XDG_POSITIONER_GRAVITY_BOTTOM_LEFT;
    case GDK_GRAVITY_WEST:
      return XDG_POSITIONER_GRAVITY_RIGHT;
    case GDK_GRAVITY_CENTER:
      return XDG_POSITIONER_GRAVITY_NONE;
    case GDK_GRAVITY_EAST:
      return XDG_POSITIONER_GRAVITY_LEFT;
    case GDK_GRAVITY_SOUTH_WEST:
      return XDG_POSITIONER_GRAVITY_TOP_RIGHT;
    case GDK_GRAVITY_SOUTH:
      return XDG_POSITIONER_GRAVITY_TOP;
    case GDK_GRAVITY_SOUTH_EAST:
      return XDG_POSITIONER_GRAVITY_TOP_LEFT;
    default:
      g_assert_not_reached ();
    }
}

static enum zxdg_positioner_v6_anchor
rect_anchor_to_anchor_legacy (GdkGravity rect_anchor)
{
  switch (rect_anchor)
    {
    case GDK_GRAVITY_NORTH_WEST:
    case GDK_GRAVITY_STATIC:
      return (ZXDG_POSITIONER_V6_ANCHOR_TOP |
              ZXDG_POSITIONER_V6_ANCHOR_LEFT);
    case GDK_GRAVITY_NORTH:
      return ZXDG_POSITIONER_V6_ANCHOR_TOP;
    case GDK_GRAVITY_NORTH_EAST:
      return (ZXDG_POSITIONER_V6_ANCHOR_TOP |
              ZXDG_POSITIONER_V6_ANCHOR_RIGHT);
    case GDK_GRAVITY_WEST:
      return ZXDG_POSITIONER_V6_ANCHOR_LEFT;
    case GDK_GRAVITY_CENTER:
      return ZXDG_POSITIONER_V6_ANCHOR_NONE;
    case GDK_GRAVITY_EAST:
      return ZXDG_POSITIONER_V6_ANCHOR_RIGHT;
    case GDK_GRAVITY_SOUTH_WEST:
      return (ZXDG_POSITIONER_V6_ANCHOR_BOTTOM |
              ZXDG_POSITIONER_V6_ANCHOR_LEFT);
    case GDK_GRAVITY_SOUTH:
      return ZXDG_POSITIONER_V6_ANCHOR_BOTTOM;
    case GDK_GRAVITY_SOUTH_EAST:
      return (ZXDG_POSITIONER_V6_ANCHOR_BOTTOM |
              ZXDG_POSITIONER_V6_ANCHOR_RIGHT);
    default:
      g_assert_not_reached ();
    }

  return (ZXDG_POSITIONER_V6_ANCHOR_TOP |
          ZXDG_POSITIONER_V6_ANCHOR_LEFT);
}

static enum zxdg_positioner_v6_gravity
surface_anchor_to_gravity_legacy (GdkGravity rect_anchor)
{
  switch (rect_anchor)
    {
    case GDK_GRAVITY_NORTH_WEST:
    case GDK_GRAVITY_STATIC:
      return (ZXDG_POSITIONER_V6_GRAVITY_BOTTOM |
              ZXDG_POSITIONER_V6_GRAVITY_RIGHT);
    case GDK_GRAVITY_NORTH:
      return ZXDG_POSITIONER_V6_GRAVITY_BOTTOM;
    case GDK_GRAVITY_NORTH_EAST:
      return (ZXDG_POSITIONER_V6_GRAVITY_BOTTOM |
              ZXDG_POSITIONER_V6_GRAVITY_LEFT);
    case GDK_GRAVITY_WEST:
      return ZXDG_POSITIONER_V6_GRAVITY_RIGHT;
    case GDK_GRAVITY_CENTER:
      return ZXDG_POSITIONER_V6_GRAVITY_NONE;
    case GDK_GRAVITY_EAST:
      return ZXDG_POSITIONER_V6_GRAVITY_LEFT;
    case GDK_GRAVITY_SOUTH_WEST:
      return (ZXDG_POSITIONER_V6_GRAVITY_TOP |
              ZXDG_POSITIONER_V6_GRAVITY_RIGHT);
    case GDK_GRAVITY_SOUTH:
      return ZXDG_POSITIONER_V6_GRAVITY_TOP;
    case GDK_GRAVITY_SOUTH_EAST:
      return (ZXDG_POSITIONER_V6_GRAVITY_TOP |
              ZXDG_POSITIONER_V6_GRAVITY_LEFT);
    default:
      g_assert_not_reached ();
    }

  return (ZXDG_POSITIONER_V6_GRAVITY_BOTTOM |
          ZXDG_POSITIONER_V6_GRAVITY_RIGHT);
}

void
gdk_wayland_toplevel_announce_csd (GdkToplevel *toplevel)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (gdk_surface_get_display (GDK_SURFACE (toplevel)));
  GdkWaylandToplevel *toplevel_wayland;

  g_return_if_fail (GDK_IS_WAYLAND_TOPLEVEL (toplevel));
  toplevel_wayland = GDK_WAYLAND_TOPLEVEL (toplevel);

  if (!display_wayland->server_decoration_manager)
    return;
  toplevel_wayland->server_decoration =
      org_kde_kwin_server_decoration_manager_create (display_wayland->server_decoration_manager,
                                                     gdk_wayland_surface_get_wl_surface (GDK_SURFACE (toplevel_wayland)));
  if (toplevel_wayland->server_decoration)
    org_kde_kwin_server_decoration_request_mode (toplevel_wayland->server_decoration,
                                                 ORG_KDE_KWIN_SERVER_DECORATION_MANAGER_MODE_CLIENT);
}

void
gdk_wayland_toplevel_announce_ssd (GdkToplevel *toplevel)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (gdk_surface_get_display (GDK_SURFACE (toplevel)));
  GdkWaylandToplevel *toplevel_wayland;

  g_return_if_fail (GDK_IS_WAYLAND_TOPLEVEL (toplevel));
  toplevel_wayland = GDK_WAYLAND_TOPLEVEL (toplevel);

  if (!display_wayland->server_decoration_manager)
    return;
  toplevel_wayland->server_decoration =
      org_kde_kwin_server_decoration_manager_create (display_wayland->server_decoration_manager,
                                                     gdk_wayland_surface_get_wl_surface (GDK_SURFACE (toplevel_wayland)));
  if (toplevel_wayland->server_decoration)
    org_kde_kwin_server_decoration_request_mode (toplevel_wayland->server_decoration,
                                                 ORG_KDE_KWIN_SERVER_DECORATION_MANAGER_MODE_SERVER);
}

gboolean
gdk_wayland_toplevel_inhibit_idle (GdkToplevel *toplevel)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (gdk_surface_get_display (GDK_SURFACE (toplevel)));
  GdkWaylandToplevel *wayland_toplevel;

  g_return_val_if_fail (GDK_IS_WAYLAND_TOPLEVEL (toplevel), FALSE);
  wayland_toplevel = GDK_WAYLAND_TOPLEVEL (toplevel);

  if (!display_wayland->idle_inhibit_manager)
    return FALSE;

  if (!wayland_toplevel->idle_inhibitor)
    {
      g_assert (wayland_toplevel->idle_inhibitor_refcount == 0);

      wayland_toplevel->idle_inhibitor =
          zwp_idle_inhibit_manager_v1_create_inhibitor (display_wayland->idle_inhibit_manager,
                                                        gdk_wayland_surface_get_wl_surface (GDK_SURFACE (wayland_toplevel)));
    }
  ++wayland_toplevel->idle_inhibitor_refcount;

  return TRUE;
}

void
gdk_wayland_toplevel_uninhibit_idle (GdkToplevel *toplevel)
{
  GdkWaylandToplevel *wayland_toplevel;

  g_return_if_fail (GDK_IS_WAYLAND_TOPLEVEL (toplevel));
  wayland_toplevel = GDK_WAYLAND_TOPLEVEL (toplevel);

  g_assert (wayland_toplevel->idle_inhibitor &&
            wayland_toplevel->idle_inhibitor_refcount > 0);

  if (--wayland_toplevel->idle_inhibitor_refcount == 0)
    {
      g_clear_pointer (&wayland_toplevel->idle_inhibitor,
                       zwp_idle_inhibitor_v1_destroy);
    }
}

static void
calculate_popup_rect (GdkSurface     *surface,
                      GdkPopupLayout *layout,
                      GdkRectangle   *out_rect)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  int width, height;
  GdkRectangle anchor_rect;
  int dx, dy;
  int shadow_left, shadow_right, shadow_top, shadow_bottom;
  int x = 0, y = 0;

  gdk_popup_layout_get_shadow_width (layout,
                                     &shadow_left,
                                     &shadow_right,
                                     &shadow_top,
                                     &shadow_bottom);

  width = (impl->popup.unconstrained_width - (shadow_left + shadow_right));
  height = (impl->popup.unconstrained_height - (shadow_top + shadow_bottom));

  anchor_rect = *gdk_popup_layout_get_anchor_rect (layout);
  gdk_popup_layout_get_offset (layout, &dx, &dy);
  anchor_rect.x += dx;
  anchor_rect.y += dy;

  switch (gdk_popup_layout_get_rect_anchor (layout))
    {
    default:
    case GDK_GRAVITY_STATIC:
    case GDK_GRAVITY_NORTH_WEST:
      x = anchor_rect.x;
      y = anchor_rect.y;
      break;
    case GDK_GRAVITY_NORTH:
      x = anchor_rect.x + (anchor_rect.width / 2);
      y = anchor_rect.y;
      break;
    case GDK_GRAVITY_NORTH_EAST:
      x = anchor_rect.x + anchor_rect.width;
      y = anchor_rect.y;
      break;
    case GDK_GRAVITY_WEST:
      x = anchor_rect.x;
      y = anchor_rect.y + (anchor_rect.height / 2);
      break;
    case GDK_GRAVITY_CENTER:
      x = anchor_rect.x + (anchor_rect.width / 2);
      y = anchor_rect.y + (anchor_rect.height / 2);
      break;
    case GDK_GRAVITY_EAST:
      x = anchor_rect.x + anchor_rect.width;
      y = anchor_rect.y + (anchor_rect.height / 2);
      break;
    case GDK_GRAVITY_SOUTH_WEST:
      x = anchor_rect.x;
      y = anchor_rect.y + anchor_rect.height;
      break;
    case GDK_GRAVITY_SOUTH:
      x = anchor_rect.x + (anchor_rect.width / 2);
      y = anchor_rect.y + anchor_rect.height;
      break;
    case GDK_GRAVITY_SOUTH_EAST:
      x = anchor_rect.x + anchor_rect.width;
      y = anchor_rect.y + anchor_rect.height;
      break;
    }

  switch (gdk_popup_layout_get_surface_anchor (layout))
    {
    default:
    case GDK_GRAVITY_STATIC:
    case GDK_GRAVITY_NORTH_WEST:
      break;
    case GDK_GRAVITY_NORTH:
      x -= width / 2;
      break;
    case GDK_GRAVITY_NORTH_EAST:
      x -= width;
      break;
    case GDK_GRAVITY_WEST:
      y -= height / 2;
      break;
    case GDK_GRAVITY_CENTER:
      x -= width / 2;
      y -= height / 2;
      break;
    case GDK_GRAVITY_EAST:
      x -= width;
      y -= height / 2;
      break;
    case GDK_GRAVITY_SOUTH_WEST:
      y -= height;
      break;
    case GDK_GRAVITY_SOUTH:
      x -= width / 2;
      y -= height;
      break;
    case GDK_GRAVITY_SOUTH_EAST:
      x -= width;
      y -= height;
      break;
    }

  *out_rect = (GdkRectangle) {
    .x = x,
    .y = y,
    .width = width,
    .height = height
  };
}

static void
update_popup_layout_state (GdkSurface     *surface,
                           int             x,
                           int             y,
                           int             width,
                           int             height,
                           GdkPopupLayout *layout)
{
  GdkRectangle best_rect;
  GdkRectangle flipped_rect;
  GdkGravity rect_anchor;
  GdkGravity surface_anchor;
  GdkAnchorHints anchor_hints;

  rect_anchor = gdk_popup_layout_get_rect_anchor (layout);
  surface_anchor = gdk_popup_layout_get_surface_anchor (layout);
  anchor_hints = gdk_popup_layout_get_anchor_hints (layout);

  calculate_popup_rect (surface, layout, &best_rect);

  flipped_rect = best_rect;

  if (x != best_rect.x &&
      anchor_hints & GDK_ANCHOR_FLIP_X)
    {
      GdkRectangle flipped_x_rect;
      GdkGravity flipped_rect_anchor;
      GdkGravity flipped_surface_anchor;
      GdkPopupLayout *flipped_layout;

      flipped_rect_anchor = gdk_gravity_flip_horizontally (rect_anchor);
      flipped_surface_anchor = gdk_gravity_flip_horizontally (surface_anchor);
      flipped_layout = gdk_popup_layout_copy (layout);
      gdk_popup_layout_set_rect_anchor (flipped_layout,
                                        flipped_rect_anchor);
      gdk_popup_layout_set_surface_anchor (flipped_layout,
                                           flipped_surface_anchor);
      calculate_popup_rect (surface,
                            flipped_layout,
                            &flipped_x_rect);
      gdk_popup_layout_unref (flipped_layout);

      if (flipped_x_rect.x == x)
        flipped_rect.x = x;
    }
  if (y != best_rect.y &&
      anchor_hints & GDK_ANCHOR_FLIP_Y)
    {
      GdkRectangle flipped_y_rect;
      GdkGravity flipped_rect_anchor;
      GdkGravity flipped_surface_anchor;
      GdkPopupLayout *flipped_layout;

      flipped_rect_anchor = gdk_gravity_flip_vertically (rect_anchor);
      flipped_surface_anchor = gdk_gravity_flip_vertically (surface_anchor);
      flipped_layout = gdk_popup_layout_copy (layout);
      gdk_popup_layout_set_rect_anchor (flipped_layout,
                                        flipped_rect_anchor);
      gdk_popup_layout_set_surface_anchor (flipped_layout,
                                           flipped_surface_anchor);
      calculate_popup_rect (surface,
                            flipped_layout,
                            &flipped_y_rect);
      gdk_popup_layout_unref (flipped_layout);

      if (flipped_y_rect.y == y)
        flipped_rect.y = y;
    }

  if (flipped_rect.x != best_rect.x)
    {
      rect_anchor = gdk_gravity_flip_horizontally (rect_anchor);
      surface_anchor = gdk_gravity_flip_horizontally (surface_anchor);
    }
  if (flipped_rect.y != best_rect.y)
    {
      rect_anchor = gdk_gravity_flip_vertically (rect_anchor);
      surface_anchor = gdk_gravity_flip_vertically (surface_anchor);
    }

  surface->popup.rect_anchor = rect_anchor;
  surface->popup.surface_anchor = surface_anchor;
}

static gpointer
create_dynamic_positioner (GdkSurface     *surface,
                           int             width,
                           int             height,
                           GdkPopupLayout *layout,
                           gboolean        ack_parent_configure)
{
  GdkSurface *parent = surface->parent;
  GdkWaylandSurface *parent_impl = GDK_WAYLAND_SURFACE (parent);
  GdkWaylandDisplay *display =
    GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));
  GdkRectangle geometry;
  uint32_t constraint_adjustment = ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_NONE;
  const GdkRectangle *anchor_rect;
  int real_anchor_rect_x, real_anchor_rect_y;
  int anchor_rect_width, anchor_rect_height;
  int rect_anchor_dx;
  int rect_anchor_dy;
  GdkGravity rect_anchor;
  GdkGravity surface_anchor;
  GdkAnchorHints anchor_hints;
  GdkRectangle parent_geometry;
  int shadow_left;
  int shadow_right;
  int shadow_top;
  int shadow_bottom;

  gdk_popup_layout_get_shadow_width (layout,
                                     &shadow_left,
                                     &shadow_right,
                                     &shadow_top,
                                     &shadow_bottom);
  geometry = (GdkRectangle) {
    .x = shadow_left,
    .y = shadow_top,
    .width = width - (shadow_left + shadow_right),
    .height = height - (shadow_top + shadow_bottom),
  };

  gdk_wayland_surface_get_window_geometry (surface->parent, &parent_geometry);

  anchor_rect = gdk_popup_layout_get_anchor_rect (layout);
  real_anchor_rect_x = anchor_rect->x - parent_geometry.x;
  real_anchor_rect_y = anchor_rect->y - parent_geometry.y;

  anchor_rect_width = MAX (anchor_rect->width, 1);
  anchor_rect_height = MAX (anchor_rect->height, 1);

  gdk_popup_layout_get_offset (layout, &rect_anchor_dx, &rect_anchor_dy);

  rect_anchor = gdk_popup_layout_get_rect_anchor (layout);
  surface_anchor = gdk_popup_layout_get_surface_anchor (layout);

  anchor_hints = gdk_popup_layout_get_anchor_hints (layout);

  switch (display->shell_variant)
    {
    case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
      {
        struct xdg_positioner *positioner;
        enum xdg_positioner_anchor anchor;
        enum xdg_positioner_gravity gravity;

        positioner = xdg_wm_base_create_positioner (display->xdg_wm_base);

        xdg_positioner_set_size (positioner, geometry.width, geometry.height);
        xdg_positioner_set_anchor_rect (positioner,
                                        real_anchor_rect_x,
                                        real_anchor_rect_y,
                                        anchor_rect_width,
                                        anchor_rect_height);
        xdg_positioner_set_offset (positioner, rect_anchor_dx, rect_anchor_dy);

        anchor = rect_anchor_to_anchor (rect_anchor);
        xdg_positioner_set_anchor (positioner, anchor);

        gravity = surface_anchor_to_gravity (surface_anchor);
        xdg_positioner_set_gravity (positioner, gravity);

        if (anchor_hints & GDK_ANCHOR_FLIP_X)
          constraint_adjustment |= XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_X;
        if (anchor_hints & GDK_ANCHOR_FLIP_Y)
          constraint_adjustment |= XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_Y;
        if (anchor_hints & GDK_ANCHOR_SLIDE_X)
          constraint_adjustment |= XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_X;
        if (anchor_hints & GDK_ANCHOR_SLIDE_Y)
          constraint_adjustment |= XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_Y;
        if (anchor_hints & GDK_ANCHOR_RESIZE_X)
          constraint_adjustment |= XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_RESIZE_X;
        if (anchor_hints & GDK_ANCHOR_RESIZE_Y)
          constraint_adjustment |= XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_RESIZE_Y;
        xdg_positioner_set_constraint_adjustment (positioner,
                                                  constraint_adjustment);

        if (xdg_positioner_get_version (positioner) >=
            XDG_POSITIONER_SET_REACTIVE_SINCE_VERSION)
          xdg_positioner_set_reactive (positioner);

        if (ack_parent_configure &&
            xdg_positioner_get_version (positioner) >=
            XDG_POSITIONER_SET_PARENT_CONFIGURE_SINCE_VERSION)
          {
            xdg_positioner_set_parent_size (positioner,
                                            parent_geometry.width,
                                            parent_geometry.height);
            xdg_positioner_set_parent_configure (positioner,
                                                 parent_impl->last_configure_serial);
          }

        return positioner;
      }
    case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
      {
        struct zxdg_positioner_v6 *positioner;
        enum zxdg_positioner_v6_anchor anchor;
        enum zxdg_positioner_v6_gravity gravity;

        positioner = zxdg_shell_v6_create_positioner (display->zxdg_shell_v6);

        zxdg_positioner_v6_set_size (positioner, geometry.width, geometry.height);
        zxdg_positioner_v6_set_anchor_rect (positioner,
                                            real_anchor_rect_x,
                                            real_anchor_rect_y,
                                            anchor_rect_width,
                                            anchor_rect_height);
        zxdg_positioner_v6_set_offset (positioner,
                                       rect_anchor_dx,
                                       rect_anchor_dy);

        anchor = rect_anchor_to_anchor_legacy (rect_anchor);
        zxdg_positioner_v6_set_anchor (positioner, anchor);

        gravity = surface_anchor_to_gravity_legacy (surface_anchor);
        zxdg_positioner_v6_set_gravity (positioner, gravity);

        if (anchor_hints & GDK_ANCHOR_FLIP_X)
          constraint_adjustment |= ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_FLIP_X;
        if (anchor_hints & GDK_ANCHOR_FLIP_Y)
          constraint_adjustment |= ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_FLIP_Y;
        if (anchor_hints & GDK_ANCHOR_SLIDE_X)
          constraint_adjustment |= ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_SLIDE_X;
        if (anchor_hints & GDK_ANCHOR_SLIDE_Y)
          constraint_adjustment |= ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_SLIDE_Y;
        if (anchor_hints & GDK_ANCHOR_RESIZE_X)
          constraint_adjustment |= ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_RESIZE_X;
        if (anchor_hints & GDK_ANCHOR_RESIZE_Y)
          constraint_adjustment |= ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_RESIZE_Y;
        zxdg_positioner_v6_set_constraint_adjustment (positioner,
                                                      constraint_adjustment);

        return positioner;
      }
    default:
      g_assert_not_reached ();
    }

  g_assert_not_reached ();
}

static gboolean
can_map_grabbing_popup (GdkSurface *surface,
                        GdkSurface *parent)
{
  GdkDisplay *display = gdk_surface_get_display (surface);
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);
  GdkSurface *top_most_popup;

  if (!display_wayland->current_grabbing_popups)
    return TRUE;

  top_most_popup = g_list_first (display_wayland->current_grabbing_popups)->data;
  return top_most_popup == parent;
}

static gboolean
gdk_wayland_surface_create_xdg_popup (GdkSurface     *surface,
                                      GdkSurface     *parent,
                                      GdkWaylandSeat *grab_input_seat,
                                      int             width,
                                      int             height,
                                      GdkPopupLayout *layout)
{
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  GdkWaylandSurface *parent_impl = GDK_WAYLAND_SURFACE (parent);
  gpointer positioner;

  if (!impl->display_server.wl_surface)
    return FALSE;

  if (!is_realized_shell_surface (parent_impl))
    return FALSE;

  if (is_realized_toplevel (impl))
    {
      g_warning ("Can't map popup, already mapped as toplevel");
      return FALSE;
    }
  if (is_realized_popup (impl))
    {
      g_warning ("Can't map popup, already mapped");
      return FALSE;
    }

  if (grab_input_seat &&
      !can_map_grabbing_popup (surface, parent))
    {
      g_warning ("Tried to map a grabbing popup with a non-top most parent");
      return FALSE;
    }

  gdk_surface_freeze_updates (surface);

  positioner = create_dynamic_positioner (surface, width, height, layout, FALSE);
  gdk_wayland_surface_create_xdg_surface_resources (surface);

  switch (display->shell_variant)
    {
    case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
      impl->display_server.xdg_popup =
        xdg_surface_get_popup (impl->display_server.xdg_surface,
                               parent_impl->display_server.xdg_surface,
                               positioner);
      xdg_popup_add_listener (impl->display_server.xdg_popup,
                              &xdg_popup_listener,
                              surface);
      xdg_positioner_destroy (positioner);
      break;
    case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
      impl->display_server.zxdg_popup_v6 =
        zxdg_surface_v6_get_popup (impl->display_server.zxdg_surface_v6,
                                   parent_impl->display_server.zxdg_surface_v6,
                                   positioner);
      zxdg_popup_v6_add_listener (impl->display_server.zxdg_popup_v6,
                                  &zxdg_popup_v6_listener,
                                  surface);
      zxdg_positioner_v6_destroy (positioner);
      break;
    default:
      g_assert_not_reached ();
    }

  gdk_popup_layout_get_shadow_width (layout,
                                     &impl->shadow_left,
                                     &impl->shadow_right,
                                     &impl->shadow_top,
                                     &impl->shadow_bottom);

  if (grab_input_seat)
    {
      struct wl_seat *seat;
      guint32 serial;

      seat = gdk_wayland_seat_get_wl_seat (GDK_SEAT (grab_input_seat));
      serial = _gdk_wayland_seat_get_last_implicit_grab_serial (grab_input_seat, NULL);

      switch (display->shell_variant)
        {
        case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
          xdg_popup_grab (impl->display_server.xdg_popup, seat, serial);
          break;
        case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
          zxdg_popup_v6_grab (impl->display_server.zxdg_popup_v6, seat, serial);
          break;
        default:
          g_assert_not_reached ();
        }
    }

  gdk_profiler_add_mark (GDK_PROFILER_CURRENT_TIME, 0, "wayland", "surface commit");
  wl_surface_commit (impl->display_server.wl_surface);

  if (GDK_IS_POPUP (surface))
    {
      g_assert (impl->popup_state == POPUP_STATE_IDLE);
      impl->popup_state = POPUP_STATE_WAITING_FOR_CONFIGURE;
      freeze_popup_toplevel_state (surface);
    }

  display->current_popups = g_list_append (display->current_popups, surface);
  if (grab_input_seat)
    {
      display->current_grabbing_popups =
        g_list_prepend (display->current_grabbing_popups, surface);
    }

  return TRUE;
}

static GdkWaylandSeat *
find_grab_input_seat (GdkSurface *surface,
                      GdkSurface *parent)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  GdkWaylandSurface *tmp_impl;

  /* Use the device that was used for the grab as the device for
   * the popup surface setup - so this relies on GTK taking the
   * grab before showing the popup surface.
   */
  if (impl->grab_input_seat)
    return GDK_WAYLAND_SEAT (impl->grab_input_seat);

  while (parent)
    {
      tmp_impl = GDK_WAYLAND_SURFACE (parent);

      if (tmp_impl->grab_input_seat)
        return GDK_WAYLAND_SEAT (tmp_impl->grab_input_seat);

      parent = parent->parent;
    }

  return NULL;
}

static void
gdk_wayland_surface_map_toplevel (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  if (!GDK_IS_WAYLAND_TOPLEVEL (surface))
    return;

  if (impl->mapped)
    return;

  gdk_wayland_surface_create_xdg_toplevel (surface);

  impl->mapped = TRUE;
}

static void
gdk_wayland_surface_show (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  if (!impl->display_server.wl_surface)
    gdk_wayland_surface_create_surface (surface);

  gdk_wayland_surface_map_toplevel (surface);
}

static void
unmap_popups_for_surface (GdkSurface *surface)
{
  GdkWaylandDisplay *display_wayland;
  GList *l;

  display_wayland = GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));
  for (l = display_wayland->current_popups; l; l = l->next)
    {
       GdkSurface *popup = l->data;

       if (popup->parent == surface)
         {
           g_warning ("Tried to unmap the parent of a popup");
           gdk_surface_hide (popup);

           return;
         }
    }
}

static void
gdk_wayland_surface_hide_surface (GdkSurface *surface)
{
  GdkDisplay *display = gdk_surface_get_display (surface);
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  unmap_popups_for_surface (surface);

  if (impl->display_server.wl_surface)
    {
      if (impl->display_server.egl_window)
        {
          gdk_surface_set_egl_native_window (surface, NULL);
          wl_egl_window_destroy (impl->display_server.egl_window);
          impl->display_server.egl_window = NULL;
        }

      if (impl->display_server.xdg_toplevel)
        {
          xdg_toplevel_destroy (impl->display_server.xdg_toplevel);
          impl->display_server.xdg_toplevel = NULL;
        }
      else if (impl->display_server.xdg_popup)
        {
          xdg_popup_destroy (impl->display_server.xdg_popup);
          impl->display_server.xdg_popup = NULL;
          display_wayland->current_popups =
            g_list_remove (display_wayland->current_popups, surface);
          display_wayland->current_grabbing_popups =
            g_list_remove (display_wayland->current_grabbing_popups, surface);
        }
      if (impl->display_server.xdg_surface)
        {
          xdg_surface_destroy (impl->display_server.xdg_surface);
          impl->display_server.xdg_surface = NULL;
          if (!impl->initial_configure_received)
            gdk_surface_thaw_updates (surface);
          else
            impl->initial_configure_received = FALSE;
        }

      if (impl->display_server.zxdg_toplevel_v6)
        {
          zxdg_toplevel_v6_destroy (impl->display_server.zxdg_toplevel_v6);
          impl->display_server.zxdg_toplevel_v6 = NULL;
        }
      else if (impl->display_server.zxdg_popup_v6)
        {
          zxdg_popup_v6_destroy (impl->display_server.zxdg_popup_v6);
          impl->display_server.zxdg_popup_v6 = NULL;
          display_wayland->current_popups =
            g_list_remove (display_wayland->current_popups, surface);
          display_wayland->current_grabbing_popups =
            g_list_remove (display_wayland->current_grabbing_popups, surface);
        }
      if (impl->display_server.zxdg_surface_v6)
        {
          zxdg_surface_v6_destroy (impl->display_server.zxdg_surface_v6);
          impl->display_server.zxdg_surface_v6 = NULL;
          if (!impl->initial_configure_received)
            gdk_surface_thaw_updates (surface);
          else
            impl->initial_configure_received = FALSE;
        }

      impl->awaiting_frame = FALSE;
      if (impl->awaiting_frame_frozen)
        {
          impl->awaiting_frame_frozen = FALSE;
          gdk_surface_thaw_updates (surface);
        }

      if (GDK_IS_POPUP (surface))
        {
          impl->popup_thaw_upon_show = TRUE;
          gdk_surface_freeze_updates (surface);

          switch (impl->popup_state)
            {
            case POPUP_STATE_WAITING_FOR_REPOSITIONED:
              gdk_surface_thaw_updates (surface);
              G_GNUC_FALLTHROUGH;
            case POPUP_STATE_WAITING_FOR_CONFIGURE:
            case POPUP_STATE_WAITING_FOR_FRAME:
              thaw_popup_toplevel_state (surface);
              break;
            case POPUP_STATE_IDLE:
              break;
            default:
              g_assert_not_reached ();
            }

          impl->popup_state = POPUP_STATE_IDLE;
        }

      if (impl->display_server.gtk_surface)
        {
          if (display_wayland->gtk_shell_version >=
              GTK_SURFACE1_RELEASE_SINCE_VERSION)
            gtk_surface1_release (impl->display_server.gtk_surface);
          else
            gtk_surface1_destroy (impl->display_server.gtk_surface);
          impl->display_server.gtk_surface = NULL;
          impl->application.was_set = FALSE;
        }

      wl_surface_destroy (impl->display_server.wl_surface);
      impl->display_server.wl_surface = NULL;

      g_slist_free (impl->display_server.outputs);
      impl->display_server.outputs = NULL;

      g_clear_pointer (&impl->toplevel.layout, gdk_toplevel_layout_unref);
      g_clear_pointer (&impl->popup.layout, gdk_popup_layout_unref);
    }

  impl->has_uncommitted_ack_configure = FALSE;
  impl->input_region_dirty = TRUE;
  impl->opaque_region_dirty = TRUE;

  unset_transient_for_exported (surface);

  impl->last_sent_window_geometry = (GdkRectangle) { 0 };
  impl->last_sent_min_width = 0;
  impl->last_sent_min_height = 0;
  impl->last_sent_max_width = 0;
  impl->last_sent_max_height = 0;

  _gdk_wayland_surface_clear_saved_size (surface);
  impl->mapped = FALSE;
}

static void
gdk_wayland_surface_hide (GdkSurface *surface)
{
  GdkSeat *seat;

  seat = gdk_display_get_default_seat (surface->display);
  if (seat)
    {
      if (surface->autohide)
        gdk_seat_ungrab (seat);

      gdk_wayland_seat_clear_touchpoints (GDK_WAYLAND_SEAT (seat), surface);
    }
  gdk_wayland_surface_hide_surface (surface);
  _gdk_surface_clear_update_area (surface);
}

static void
gdk_wayland_surface_move_resize (GdkSurface *surface,
                                 int         x,
                                 int         y,
                                 int         width,
                                 int         height)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  surface->x = x;
  surface->y = y;
  gdk_wayland_surface_maybe_resize (surface, width, height, impl->scale);
}

static gboolean
is_fallback_relayout_possible (GdkSurface *surface)
{
  GList *l;

  for (l = surface->children; l; l = l->next)
    {
      GdkSurface *child = l->data;

      if (GDK_WAYLAND_SURFACE (child)->mapped)
        return FALSE;
    }

  return TRUE;
}

static gboolean gdk_wayland_surface_present_popup (GdkSurface     *surface,
                                                   int             width,
                                                   int             height,
                                                   GdkPopupLayout *layout);

static void
queue_relayout_fallback (GdkSurface     *surface,
                         GdkPopupLayout *layout)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  if (!is_fallback_relayout_possible (surface))
    return;

  gdk_wayland_surface_hide_surface (surface);
  gdk_wayland_surface_present_popup (surface,
                                     impl->popup.unconstrained_width,
                                     impl->popup.unconstrained_height,
                                     layout);
}

static void
do_queue_relayout (GdkSurface     *surface,
                   int             width,
                   int             height,
                   GdkPopupLayout *layout)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  struct xdg_positioner *positioner;

  g_assert (is_realized_popup (impl));
  g_assert (impl->popup_state == POPUP_STATE_IDLE ||
            impl->popup_state == POPUP_STATE_WAITING_FOR_FRAME);

  g_clear_pointer (&impl->popup.layout, gdk_popup_layout_unref);
  impl->popup.layout = gdk_popup_layout_copy (layout);
  impl->popup.unconstrained_width = width;
  impl->popup.unconstrained_height = height;

  if (!impl->display_server.xdg_popup ||
      xdg_popup_get_version (impl->display_server.xdg_popup) <
      XDG_POPUP_REPOSITION_SINCE_VERSION)
    {
      g_warning_once ("Compositor doesn't support moving popups, "
                      "relying on remapping");
      queue_relayout_fallback (surface, layout);

      return;
    }

  positioner = create_dynamic_positioner (surface,
                                          width, height, layout,
                                          TRUE);
  xdg_popup_reposition (impl->display_server.xdg_popup,
                        positioner,
                        ++impl->reposition_token);
  xdg_positioner_destroy (positioner);

  gdk_surface_freeze_updates (surface);

  switch (impl->popup_state)
    {
    case POPUP_STATE_IDLE:
      freeze_popup_toplevel_state (surface);
      break;
    case POPUP_STATE_WAITING_FOR_FRAME:
      break;
    case POPUP_STATE_WAITING_FOR_CONFIGURE:
    case POPUP_STATE_WAITING_FOR_REPOSITIONED:
    default:
      g_assert_not_reached ();
    }

  impl->popup_state = POPUP_STATE_WAITING_FOR_REPOSITIONED;
}

static gboolean
is_relayout_finished (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  if (!impl->initial_configure_received)
    return FALSE;

  if (impl->reposition_token != impl->received_reposition_token)
    return FALSE;

  return TRUE;
}

static void
gdk_wayland_surface_map_popup (GdkSurface     *surface,
                               int             width,
                               int             height,
                               GdkPopupLayout *layout)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  GdkSurface *parent;
  GdkWaylandSeat *grab_input_seat;

  parent = surface->parent;
  if (!parent)
    {
      g_warning ("Couldn't map as surface %p as popup because it doesn't have a parent",
                 surface);
      return;
    }

  if (surface->autohide)
    grab_input_seat = find_grab_input_seat (surface, parent);
  else
    grab_input_seat = NULL;

  if (!gdk_wayland_surface_create_xdg_popup (surface,
                                             parent,
                                             grab_input_seat,
                                             width, height,
                                             layout))
    return;

  impl->popup.layout = gdk_popup_layout_copy (layout);
  impl->popup.unconstrained_width = width;
  impl->popup.unconstrained_height = height;
  impl->mapped = TRUE;
}

static void
show_popup (GdkSurface     *surface,
            int             width,
            int             height,
            GdkPopupLayout *layout)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  if (!impl->display_server.wl_surface)
    gdk_wayland_surface_create_surface (surface);

  if (impl->popup_thaw_upon_show)
    {
      impl->popup_thaw_upon_show = FALSE;
      gdk_surface_thaw_updates (surface);
    }

  gdk_wayland_surface_map_popup (surface, width, height, layout);
}

typedef struct
{
  int width;
  int height;
  GdkPopupLayout *layout;
} GrabPrepareData;

static void
show_grabbing_popup (GdkSeat    *seat,
                     GdkSurface *surface,
                     gpointer    user_data)
{
  GrabPrepareData *data = user_data;

  show_popup (surface, data->width, data->height, data->layout);
}

static void
reposition_popup (GdkSurface     *surface,
                  int             width,
                  int             height,
                  GdkPopupLayout *layout)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  switch (impl->popup_state)
    {
    case POPUP_STATE_IDLE:
    case POPUP_STATE_WAITING_FOR_FRAME:
      do_queue_relayout (surface, width, height, layout);
      break;
    case POPUP_STATE_WAITING_FOR_REPOSITIONED:
    case POPUP_STATE_WAITING_FOR_CONFIGURE:
      g_warn_if_reached ();
      break;
    default:
      g_assert_not_reached ();
    }
}

static gboolean
gdk_wayland_surface_present_popup (GdkSurface     *surface,
                                   int             width,
                                   int             height,
                                   GdkPopupLayout *layout)
{
  GdkWaylandDisplay *display_wayland =
    GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));
  GdkWaylandSurface *impl;

  impl = GDK_WAYLAND_SURFACE (surface);

  if (!impl->mapped)
    {
      if (surface->autohide)
        {
          GdkSeat *seat;

          seat = gdk_display_get_default_seat (surface->display);
          if (seat)
            {
              GrabPrepareData data;
              GdkGrabStatus result;

              data = (GrabPrepareData) {
                .width = width,
                .height = height,
                .layout = layout,
              };

              result = gdk_seat_grab (seat,
                                      surface,
                                      GDK_SEAT_CAPABILITY_ALL,
                                      TRUE,
                                      NULL, NULL,
                                      show_grabbing_popup, &data);
              if (result != GDK_GRAB_SUCCESS)
                {
                  const char *grab_status[] = {
                    "success", "already grabbed", "invalid time",
                    "not viewable", "frozen", "failed"
                  };
                  g_warning ("Grab failed: %s", grab_status[result]);
                }
            }
        }
      else
        {
          show_popup (surface, width, height, layout);
        }
    }
  else
    {
      if (impl->popup.unconstrained_width == width &&
          impl->popup.unconstrained_height == height &&
          gdk_popup_layout_equal (impl->popup.layout, layout))
        return TRUE;

      reposition_popup (surface, width, height, layout);
    }

  while (impl->display_server.xdg_popup && !is_relayout_finished (surface))
    wl_display_dispatch_queue (display_wayland->wl_display, impl->event_queue);

  if (impl->display_server.xdg_popup)
    {
      gdk_surface_invalidate_rect (surface, NULL);
      return TRUE;
    }
  else
    {
      return FALSE;
    }
}

static void
gdk_wayland_surface_get_geometry (GdkSurface *surface,
                                  int        *x,
                                  int        *y,
                                  int        *width,
                                  int        *height)
{
  if (!GDK_SURFACE_DESTROYED (surface))
    {
      if (x)
        *x = surface->x;
      if (y)
        *y = surface->y;
      if (width)
        *width = surface->width;
      if (height)
        *height = surface->height;
    }
}

static void
gdk_wayland_surface_get_root_coords (GdkSurface *surface,
                                     int         x,
                                     int         y,
                                     int        *root_x,
                                     int        *root_y)
{
  /*
   * Wayland does not have a global coordinate space shared between surfaces. In
   * fact, for regular toplevels, we have no idea where our surfaces are
   * positioned, relatively.
   *
   * However, there are some cases like popups and subsurfaces where we do have
   * some amount of control over the placement of our surface, and we can
   * semi-accurately control the x/y position of these surfaces, if they are
   * relative to another surface.
   *
   * To pretend we have something called a root coordinate space, assume all
   * parent-less surfaces are positioned in (0, 0), and all relative positioned
   * popups and subsurfaces are placed within this fake root coordinate space.
   *
   * For example a 200x200 large toplevel surface will have the position (0, 0).
   * If a popup positioned in the middle of the toplevel will have the fake
   * position (100,100). Furthermore, if a positioned is placed in the middle
   * that popup, will have the fake position (150,150), even though it has the
   * relative position (50,50). These three surfaces would make up one single
   * fake root coordinate space.
   */

  if (root_x)
    *root_x = surface->x + x;

  if (root_y)
    *root_y = surface->y + y;
}

static gboolean
gdk_wayland_surface_get_device_state (GdkSurface       *surface,
                                      GdkDevice        *device,
                                      double           *x,
                                      double           *y,
                                      GdkModifierType  *mask)
{
  if (GDK_SURFACE_DESTROYED (surface))
    return FALSE;

  gdk_wayland_device_query_state (device, surface, x, y, mask);

  return *x >= 0 && *y >= 0 && *x < surface->width && *y < surface->height;
}

static void
gdk_wayland_surface_set_input_region (GdkSurface     *surface,
                                      cairo_region_t *input_region)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  g_clear_pointer (&impl->input_region, cairo_region_destroy);

  if (input_region)
    impl->input_region = cairo_region_copy (input_region);

  impl->input_region_dirty = TRUE;
}

static void
gdk_wayland_surface_destroy (GdkSurface *surface,
                             gboolean    foreign_destroy)
{
  GdkWaylandDisplay *display;
  GdkFrameClock *frame_clock;

  g_return_if_fail (GDK_IS_SURFACE (surface));

  /* Wayland surfaces can't be externally destroyed; we may possibly
   * eventually want to use this path at display close-down
   */
  g_return_if_fail (!foreign_destroy);

  gdk_wayland_surface_hide_surface (surface);

  frame_clock = gdk_surface_get_frame_clock (surface);
  g_signal_handlers_disconnect_by_func (frame_clock, on_frame_clock_before_paint, surface);
  g_signal_handlers_disconnect_by_func (frame_clock, on_frame_clock_after_paint, surface);

  display = GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));
  display->toplevels = g_list_remove (display->toplevels, surface);
}

static void
token_done (gpointer                        data,
            struct xdg_activation_token_v1 *provider,
            const char                     *token)
{
  char **token_out = data;

  *token_out = g_strdup (token);
}

static const struct xdg_activation_token_v1_listener token_listener = {
  token_done,
};

static void
gdk_wayland_surface_focus (GdkSurface *surface,
                           guint32     timestamp)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  GdkDisplay *display = gdk_surface_get_display (surface);
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);
  gchar *startup_id = NULL;

  startup_id = g_steal_pointer (&display_wayland->startup_notification_id);

  if (display_wayland->xdg_activation)
    {
      GdkWaylandSeat *seat =
        GDK_WAYLAND_SEAT (gdk_display_get_default_seat (display));

      /* If the focus request does not have a startup ID associated, get a
       * new token to activate the window.
       */
      if (!startup_id)
        {
          struct xdg_activation_token_v1 *token;
          struct wl_event_queue *event_queue;

          event_queue = wl_display_create_queue (display_wayland->wl_display);

          token = xdg_activation_v1_get_activation_token (display_wayland->xdg_activation);
          wl_proxy_set_queue ((struct wl_proxy *) token, event_queue);

          xdg_activation_token_v1_add_listener (token,
                                                &token_listener,
                                                &startup_id);
          xdg_activation_token_v1_set_serial (token,
                                              _gdk_wayland_seat_get_last_implicit_grab_serial (seat, NULL),
                                              gdk_wayland_seat_get_wl_seat (GDK_SEAT (seat)));
          xdg_activation_token_v1_set_surface (token,
                                               gdk_wayland_surface_get_wl_surface (surface));
          xdg_activation_token_v1_commit (token);

          while (startup_id == NULL)
            wl_display_dispatch_queue (display_wayland->wl_display, event_queue);

          xdg_activation_token_v1_destroy (token);
          wl_event_queue_destroy (event_queue);
        }

      xdg_activation_v1_activate (display_wayland->xdg_activation,
                                  startup_id,
                                  impl->display_server.wl_surface);
    }
  else if (impl->display_server.gtk_surface)
    {
      if (timestamp != GDK_CURRENT_TIME)
        gtk_surface1_present (impl->display_server.gtk_surface, timestamp);
      else if (startup_id && display_wayland->gtk_shell_version >= 3)
        gtk_surface1_request_focus (impl->display_server.gtk_surface,
                                    startup_id);
    }

  g_free (startup_id);
}

static void
gtk_surface_configure (void                *data,
                       struct gtk_surface1 *gtk_surface,
                       struct wl_array     *states)
{
  GdkSurface *surface = GDK_SURFACE (data);
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  GdkToplevelState new_state = 0;
  uint32_t *p;

  wl_array_for_each (p, states)
    {
      uint32_t state = *p;

      switch (state)
        {
        case GTK_SURFACE1_STATE_TILED:
          new_state |= GDK_TOPLEVEL_STATE_TILED;
          break;

        /* Since v2 */
        case GTK_SURFACE1_STATE_TILED_TOP:
          new_state |= (GDK_TOPLEVEL_STATE_TILED | GDK_TOPLEVEL_STATE_TOP_TILED);
          break;
        case GTK_SURFACE1_STATE_TILED_RIGHT:
          new_state |= (GDK_TOPLEVEL_STATE_TILED | GDK_TOPLEVEL_STATE_RIGHT_TILED);
          break;
        case GTK_SURFACE1_STATE_TILED_BOTTOM:
          new_state |= (GDK_TOPLEVEL_STATE_TILED | GDK_TOPLEVEL_STATE_BOTTOM_TILED);
          break;
        case GTK_SURFACE1_STATE_TILED_LEFT:
          new_state |= (GDK_TOPLEVEL_STATE_TILED | GDK_TOPLEVEL_STATE_LEFT_TILED);
          break;
        default:
          /* Unknown state */
          break;
        }
    }

  impl->pending.toplevel.state |= new_state;
}

static void
gtk_surface_configure_edges (void                *data,
                             struct gtk_surface1 *gtk_surface,
                             struct wl_array     *edge_constraints)
{
  GdkSurface *surface = GDK_SURFACE (data);
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  GdkToplevelState new_state = 0;
  uint32_t *p;

  wl_array_for_each (p, edge_constraints)
    {
      uint32_t constraint = *p;

      switch (constraint)
        {
        case GTK_SURFACE1_EDGE_CONSTRAINT_RESIZABLE_TOP:
          new_state |= GDK_TOPLEVEL_STATE_TOP_RESIZABLE;
          break;
        case GTK_SURFACE1_EDGE_CONSTRAINT_RESIZABLE_RIGHT:
          new_state |= GDK_TOPLEVEL_STATE_RIGHT_RESIZABLE;
          break;
        case GTK_SURFACE1_EDGE_CONSTRAINT_RESIZABLE_BOTTOM:
          new_state |= GDK_TOPLEVEL_STATE_BOTTOM_RESIZABLE;
          break;
        case GTK_SURFACE1_EDGE_CONSTRAINT_RESIZABLE_LEFT:
          new_state |= GDK_TOPLEVEL_STATE_LEFT_RESIZABLE;
          break;
        default:
          /* Unknown state */
          break;
        }
    }

  impl->pending.toplevel.state |= new_state;
}

static const struct gtk_surface1_listener gtk_surface_listener = {
  gtk_surface_configure,
  gtk_surface_configure_edges
};

static void
gdk_wayland_surface_init_gtk_surface (GdkWaylandSurface *impl)
{
  GdkWaylandDisplay *display =
    GDK_WAYLAND_DISPLAY (gdk_surface_get_display (GDK_SURFACE (impl)));

  if (impl->display_server.gtk_surface != NULL)
    return;
  if (!is_realized_toplevel (impl))
    return;
  if (display->gtk_shell == NULL)
    return;

  impl->display_server.gtk_surface =
    gtk_shell1_get_gtk_surface (display->gtk_shell,
                                impl->display_server.wl_surface);
  wl_proxy_set_queue ((struct wl_proxy *) impl->display_server.gtk_surface,
                      impl->event_queue);
  gdk_wayland_surface_set_geometry_hints (impl,
                                          &impl->geometry_hints,
                                          impl->geometry_mask);
  gtk_surface1_add_listener (impl->display_server.gtk_surface,
                             &gtk_surface_listener,
                             impl);
}

static void
maybe_set_gtk_surface_modal (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  gdk_wayland_surface_init_gtk_surface (impl);
  if (impl->display_server.gtk_surface == NULL)
    return;

  if (surface->modal_hint)
    gtk_surface1_set_modal (impl->display_server.gtk_surface);
  else
    gtk_surface1_unset_modal (impl->display_server.gtk_surface);

}

static void
gdk_wayland_surface_set_modal_hint (GdkSurface *surface,
                                    gboolean    modal)
{
  surface->modal_hint = modal;
  maybe_set_gtk_surface_modal (surface);
}

static void
gdk_wayland_surface_set_geometry_hints (GdkWaylandSurface  *impl,
                                        const GdkGeometry  *geometry,
                                        GdkSurfaceHints     geom_mask)
{
  GdkWaylandDisplay *display_wayland;
  int min_width, min_height;
  int max_width, max_height;

  if (GDK_SURFACE_DESTROYED (impl) ||
      !SURFACE_IS_TOPLEVEL (impl))
    return;

  display_wayland = GDK_WAYLAND_DISPLAY (gdk_surface_get_display (GDK_SURFACE (impl)));

  impl->geometry_hints = *geometry;
  impl->geometry_mask = geom_mask;

  if (!is_realized_toplevel (impl))
    return;

  if (geom_mask & GDK_HINT_MIN_SIZE)
    {
      min_width = MAX (0, (geometry->min_width -
                           (impl->shadow_left + impl->shadow_right)));
      min_height = MAX (0, (geometry->min_height -
                            (impl->shadow_top + impl->shadow_bottom)));
    }
  else
    {
      min_width = 0;
      min_height = 0;
    }

  if (geom_mask & GDK_HINT_MAX_SIZE)
    {
      max_width = MAX (0, (geometry->max_width -
                           (impl->shadow_left + impl->shadow_right)));
      max_height = MAX (0, (geometry->max_height -
                            (impl->shadow_top + impl->shadow_bottom)));
    }
  else
    {
      max_width = 0;
      max_height = 0;
    }

  if (impl->last_sent_min_width == min_width &&
      impl->last_sent_min_height == min_height &&
      impl->last_sent_max_width == max_width &&
      impl->last_sent_max_height == max_height)
    return;

  switch (display_wayland->shell_variant)
    {
    case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
      xdg_toplevel_set_min_size (impl->display_server.xdg_toplevel,
                                 min_width, min_height);
      xdg_toplevel_set_max_size (impl->display_server.xdg_toplevel,
                                 max_width, max_height);
      break;
    case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
      zxdg_toplevel_v6_set_min_size (impl->display_server.zxdg_toplevel_v6,
                                     min_width, min_height);
      zxdg_toplevel_v6_set_max_size (impl->display_server.zxdg_toplevel_v6,
                                     max_width, max_height);
      break;
    default:
      g_assert_not_reached ();
    }

  impl->last_sent_min_width = min_width;
  impl->last_sent_min_height = min_height;
  impl->last_sent_max_width = max_width;
  impl->last_sent_max_height = max_height;
}

static void
gdk_wayland_surface_set_title (GdkSurface  *surface,
                               const char *title)
{
  GdkWaylandSurface *impl;
  const char *end;
  gsize title_length;

  g_return_if_fail (title != NULL);

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  impl = GDK_WAYLAND_SURFACE (surface);

  if (g_strcmp0 (impl->title, title) == 0)
    return;

  g_free (impl->title);

  title_length = MIN (strlen (title), MAX_WL_BUFFER_SIZE);
  if (g_utf8_validate (title, title_length, &end))
    {
      impl->title = g_malloc (end - title + 1);
      memcpy (impl->title, title, end - title);
      impl->title[end - title] = '\0';
    }
  else
    {
      impl->title = g_utf8_make_valid (title, title_length);
      g_warning ("Invalid utf8 passed to gdk_surface_set_title: '%s'", title);
    }

  gdk_wayland_surface_sync_title (surface);
}

static void
gdk_wayland_surface_set_startup_id (GdkSurface  *surface,
                                    const char *startup_id)
{
}

static gboolean
check_transient_for_loop (GdkWaylandToplevel *toplevel,
                          GdkWaylandToplevel *parent)
{
  while (parent)
    {
      if (parent->transient_for == toplevel)
        return TRUE;
      parent = parent->transient_for;
    }
  return FALSE;
}

static void
gdk_wayland_toplevel_set_transient_for (GdkWaylandToplevel *toplevel,
                                        GdkSurface         *parent)
{
  g_return_if_fail (!parent || GDK_IS_WAYLAND_TOPLEVEL (parent));
  g_return_if_fail (!parent ||
                    gdk_surface_get_display (GDK_SURFACE (toplevel)) == gdk_surface_get_display (parent));

  if (parent)
    {
      GdkWaylandToplevel *parent_toplevel = GDK_WAYLAND_TOPLEVEL (parent);

      if (check_transient_for_loop (toplevel, parent_toplevel))
        {
          g_warning ("Setting %p transient for %p would create a loop",
                     toplevel, parent);
          return;
        }
    }

  unset_transient_for_exported (GDK_SURFACE (toplevel));

  if (parent)
    toplevel->transient_for = GDK_WAYLAND_TOPLEVEL (parent);
  else
    toplevel->transient_for = NULL;

  gdk_wayland_surface_sync_parent (GDK_SURFACE (toplevel), NULL);
}

static void
gdk_wayland_surface_minimize (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  GdkWaylandDisplay *display_wayland;

  if (GDK_SURFACE_DESTROYED (surface) ||
      !SURFACE_IS_TOPLEVEL (surface))
    return;

  if (!is_realized_toplevel (GDK_WAYLAND_SURFACE (surface)))
    return;

  /* FIXME: xdg_toplevel does not come with a minimized state that we can
   * query or get notified of. This means we cannot implement the full
   * GdkSurface API, and our state will not reflect minimization.
   */
  display_wayland = GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));
  switch (display_wayland->shell_variant)
    {
    case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
      xdg_toplevel_set_minimized (impl->display_server.xdg_toplevel);
      break;
    case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
      zxdg_toplevel_v6_set_minimized (impl->display_server.zxdg_toplevel_v6);
      break;
    default:
      g_assert_not_reached ();
    }
}

static void
gdk_wayland_surface_maximize (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  _gdk_wayland_surface_save_size (surface);

  if (is_realized_toplevel (impl))
    {
      GdkWaylandDisplay *display_wayland =
        GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));

      switch (display_wayland->shell_variant)
        {
        case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
          xdg_toplevel_set_maximized (impl->display_server.xdg_toplevel);
          break;
        case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
          zxdg_toplevel_v6_set_maximized (impl->display_server.zxdg_toplevel_v6);
          break;
        default:
          g_assert_not_reached ();
        }
    }
  else
    {
      synthesize_initial_surface_state (surface, 0, GDK_TOPLEVEL_STATE_MAXIMIZED);
    }
}

static void
gdk_wayland_surface_unmaximize (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  if (is_realized_toplevel (impl))
    {
      GdkWaylandDisplay *display_wayland =
        GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));

      switch (display_wayland->shell_variant)
        {
        case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
          xdg_toplevel_unset_maximized (impl->display_server.xdg_toplevel);
          break;
        case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
          zxdg_toplevel_v6_unset_maximized (impl->display_server.zxdg_toplevel_v6);
          break;
        default:
          g_assert_not_reached ();
        }
    }
  else
    {
      synthesize_initial_surface_state (surface, GDK_TOPLEVEL_STATE_MAXIMIZED, 0);
    }
}

static void
gdk_wayland_surface_fullscreen_on_monitor (GdkSurface *surface,
                                           GdkMonitor *monitor)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  struct wl_output *output = ((GdkWaylandMonitor *)monitor)->output;

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  _gdk_wayland_surface_save_size (surface);

  if (is_realized_toplevel (impl))
    {
      GdkWaylandDisplay *display_wayland =
        GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));

      switch (display_wayland->shell_variant)
        {
        case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
          xdg_toplevel_set_fullscreen (impl->display_server.xdg_toplevel,
                                       output);
          break;
        case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
          zxdg_toplevel_v6_set_fullscreen (impl->display_server.zxdg_toplevel_v6,
                                           output);
          break;
        default:
          g_assert_not_reached ();
        }
    }
  else
    {
      synthesize_initial_surface_state (surface, 0, GDK_TOPLEVEL_STATE_FULLSCREEN);
      impl->initial_fullscreen_output = output;
    }
}

static void
gdk_wayland_surface_fullscreen (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  impl->initial_fullscreen_output = NULL;

  _gdk_wayland_surface_save_size (surface);

  if (is_realized_toplevel (impl))
    {
      GdkWaylandDisplay *display_wayland =
        GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));

      switch (display_wayland->shell_variant)
        {
        case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
          xdg_toplevel_set_fullscreen (impl->display_server.xdg_toplevel,
                                       NULL);
          break;
        case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
          zxdg_toplevel_v6_set_fullscreen (impl->display_server.zxdg_toplevel_v6,
                                           NULL);
          break;
        default:
          g_assert_not_reached ();
        }
    }
  else
    {
      synthesize_initial_surface_state (surface, 0, GDK_TOPLEVEL_STATE_FULLSCREEN);
    }
}

static void
gdk_wayland_surface_unfullscreen (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  impl->initial_fullscreen_output = NULL;

  if (is_realized_toplevel (impl))
    {
      GdkWaylandDisplay *display_wayland =
        GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));

      switch (display_wayland->shell_variant)
        {
        case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
          xdg_toplevel_unset_fullscreen (impl->display_server.xdg_toplevel);
          break;
        case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
          zxdg_toplevel_v6_unset_fullscreen (impl->display_server.zxdg_toplevel_v6);
          break;
        default:
          g_assert_not_reached ();
        }
    }
  else
    {
      synthesize_initial_surface_state (surface, GDK_TOPLEVEL_STATE_FULLSCREEN, 0);
    }
}

static void
gdk_wayland_toplevel_begin_resize (GdkToplevel    *toplevel,
                                   GdkSurfaceEdge  edge,
                                   GdkDevice      *device,
                                   int             button,
                                   double          x,
                                   double          y,
                                   guint32         timestamp)
{
  GdkSurface *surface = GDK_SURFACE (toplevel);
  GdkWaylandSurface *impl;
  GdkWaylandDisplay *display_wayland;
  GdkEventSequence *sequence;
  uint32_t resize_edges, serial;

  if (GDK_SURFACE_DESTROYED (surface) ||
      !SURFACE_IS_TOPLEVEL (surface))
    return;

  switch (edge)
    {
    case GDK_SURFACE_EDGE_NORTH_WEST:
      resize_edges = ZXDG_TOPLEVEL_V6_RESIZE_EDGE_TOP_LEFT;
      break;

    case GDK_SURFACE_EDGE_NORTH:
      resize_edges = ZXDG_TOPLEVEL_V6_RESIZE_EDGE_TOP;
      break;

    case GDK_SURFACE_EDGE_NORTH_EAST:
      resize_edges = ZXDG_TOPLEVEL_V6_RESIZE_EDGE_TOP_RIGHT;
      break;

    case GDK_SURFACE_EDGE_WEST:
      resize_edges = ZXDG_TOPLEVEL_V6_RESIZE_EDGE_LEFT;
      break;

    case GDK_SURFACE_EDGE_EAST:
      resize_edges = ZXDG_TOPLEVEL_V6_RESIZE_EDGE_RIGHT;
      break;

    case GDK_SURFACE_EDGE_SOUTH_WEST:
      resize_edges = ZXDG_TOPLEVEL_V6_RESIZE_EDGE_BOTTOM_LEFT;
      break;

    case GDK_SURFACE_EDGE_SOUTH:
      resize_edges = ZXDG_TOPLEVEL_V6_RESIZE_EDGE_BOTTOM;
      break;

    case GDK_SURFACE_EDGE_SOUTH_EAST:
      resize_edges = ZXDG_TOPLEVEL_V6_RESIZE_EDGE_BOTTOM_RIGHT;
      break;

    default:
      g_warning ("gdk_toplevel_begin_resize: bad resize edge %d!", edge);
      return;
    }

  impl = GDK_WAYLAND_SURFACE (surface);
  display_wayland = GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));

  if (!is_realized_toplevel (impl))
    return;

  serial = _gdk_wayland_seat_get_last_implicit_grab_serial (GDK_WAYLAND_SEAT (gdk_device_get_seat (device)),
                                                            &sequence);

  switch (display_wayland->shell_variant)
    {
    case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
      xdg_toplevel_resize (impl->display_server.xdg_toplevel,
                           gdk_wayland_device_get_wl_seat (device),
                           serial, resize_edges);
      break;
    case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
      zxdg_toplevel_v6_resize (impl->display_server.zxdg_toplevel_v6,
                               gdk_wayland_device_get_wl_seat (device),
                               serial, resize_edges);
      break;
    default:
      g_assert_not_reached ();
    }

  if (sequence)
    gdk_wayland_device_unset_touch_grab (device, sequence);
}

static void
gdk_wayland_toplevel_begin_move (GdkToplevel *toplevel,
                                 GdkDevice   *device,
                                 int          button,
                                 double       x,
                                 double       y,
                                 guint32      timestamp)
{
  GdkSurface *surface = GDK_SURFACE (toplevel);
  GdkWaylandSurface *impl;
  GdkWaylandDisplay *display_wayland;
  GdkEventSequence *sequence;
  uint32_t serial;

  if (GDK_SURFACE_DESTROYED (surface) ||
      !SURFACE_IS_TOPLEVEL (surface))
    return;

  impl = GDK_WAYLAND_SURFACE (surface);
  display_wayland = GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));

  if (!is_realized_toplevel (impl))
    return;

  serial = _gdk_wayland_seat_get_last_implicit_grab_serial (GDK_WAYLAND_SEAT (gdk_device_get_seat (device)),
                                                            &sequence);
  switch (display_wayland->shell_variant)
    {
    case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
      xdg_toplevel_move (impl->display_server.xdg_toplevel,
                         gdk_wayland_device_get_wl_seat (device),
                         serial);
      break;
    case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
      zxdg_toplevel_v6_move (impl->display_server.zxdg_toplevel_v6,
                             gdk_wayland_device_get_wl_seat (device),
                             serial);
      break;
    default:
      g_assert_not_reached ();
    }

  if (sequence)
    gdk_wayland_device_unset_touch_grab (device, sequence);
}

static void
gdk_wayland_surface_destroy_notify (GdkSurface *surface)
{
  if (!GDK_SURFACE_DESTROYED (surface))
    {
      g_warning ("GdkSurface %p unexpectedly destroyed", surface);
      _gdk_surface_destroy (surface, TRUE);
    }

  g_object_unref (surface);
}

static int
gdk_wayland_surface_get_scale_factor (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  if (GDK_SURFACE_DESTROYED (surface))
    return 1;

  return impl->scale;
}

static void
gdk_wayland_surface_set_opaque_region (GdkSurface     *surface,
                                       cairo_region_t *region)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  g_clear_pointer (&impl->opaque_region, cairo_region_destroy);
  impl->opaque_region = cairo_region_reference (region);
  impl->opaque_region_dirty = TRUE;
}

static gboolean
gdk_wayland_surface_show_window_menu (GdkSurface *surface,
                                      GdkEvent   *event)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  GdkWaylandDisplay *display_wayland =
    GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));
  GdkSeat *seat;
  struct wl_seat *wl_seat;
  double x, y;
  uint32_t serial;

  GdkEventType event_type = gdk_event_get_event_type (event);
  switch ((guint) event_type)
    {
    case GDK_BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
    case GDK_TOUCH_BEGIN:
    case GDK_TOUCH_END:
      break;
    default:
      return FALSE;
    }

  if (!is_realized_toplevel (impl))
    return FALSE;

  seat = gdk_event_get_seat (event);
  wl_seat = gdk_wayland_seat_get_wl_seat (seat);
  gdk_event_get_position (event, &x, &y);

  serial = _gdk_wayland_seat_get_implicit_grab_serial (seat, event);

  switch (display_wayland->shell_variant)
    {
    case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
      xdg_toplevel_show_window_menu (impl->display_server.xdg_toplevel,
                                     wl_seat, serial, x, y);
      break;
    case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
      zxdg_toplevel_v6_show_window_menu (impl->display_server.zxdg_toplevel_v6,
                                         wl_seat, serial, x, y);
      break;
    default:
      g_assert_not_reached ();
    }

  return TRUE;
}

static gboolean
translate_gesture (GdkTitlebarGesture         gesture,
                   enum gtk_surface1_gesture *out_gesture)
{
  switch (gesture)
    {
    case GDK_TITLEBAR_GESTURE_DOUBLE_CLICK:
      *out_gesture = GTK_SURFACE1_GESTURE_DOUBLE_CLICK;
      break;

    case GDK_TITLEBAR_GESTURE_RIGHT_CLICK:
      *out_gesture = GTK_SURFACE1_GESTURE_RIGHT_CLICK;
      break;

    case GDK_TITLEBAR_GESTURE_MIDDLE_CLICK:
      *out_gesture = GTK_SURFACE1_GESTURE_MIDDLE_CLICK;
      break;

    default:
      g_warning ("Not handling unknown titlebar gesture %u", gesture);
      return FALSE;
    }

  return TRUE;
}

static gboolean
gdk_wayland_surface_titlebar_gesture (GdkSurface         *surface,
                                      GdkTitlebarGesture  gesture)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  struct gtk_surface1 *gtk_surface = impl->display_server.gtk_surface;
  enum gtk_surface1_gesture gtk_gesture;
  GdkSeat *seat;
  struct wl_seat *wl_seat;
  uint32_t serial;

  if (!gtk_surface)
    return FALSE;

  if (gtk_surface1_get_version (gtk_surface) < GTK_SURFACE1_TITLEBAR_GESTURE_SINCE_VERSION)
    return FALSE;

  if (!translate_gesture (gesture, &gtk_gesture))
    return FALSE;

  seat = gdk_display_get_default_seat (surface->display);
  wl_seat = gdk_wayland_seat_get_wl_seat (seat);

  serial = _gdk_wayland_seat_get_last_implicit_grab_serial (GDK_WAYLAND_SEAT (seat), NULL);

  gtk_surface1_titlebar_gesture (impl->display_server.gtk_surface,
                                 serial,
                                 wl_seat,
                                 gtk_gesture);

  return TRUE;
}

static gboolean
gdk_wayland_surface_supports_edge_constraints (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  struct gtk_surface1 *gtk_surface = impl->display_server.gtk_surface;

  if (!gtk_surface)
    return FALSE;

  return gtk_surface1_get_version (gtk_surface) >= GTK_SURFACE1_CONFIGURE_EDGES_SINCE_VERSION;
}

static void
gdk_wayland_surface_class_init (GdkWaylandSurfaceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkSurfaceClass *impl_class = GDK_SURFACE_CLASS (klass);

  object_class->constructed = gdk_wayland_surface_constructed;
  object_class->dispose = gdk_wayland_surface_dispose;
  object_class->finalize = gdk_wayland_surface_finalize;

  impl_class->hide = gdk_wayland_surface_hide;
  impl_class->get_geometry = gdk_wayland_surface_get_geometry;
  impl_class->get_root_coords = gdk_wayland_surface_get_root_coords;
  impl_class->get_device_state = gdk_wayland_surface_get_device_state;
  impl_class->set_input_region = gdk_wayland_surface_set_input_region;
  impl_class->destroy = gdk_wayland_surface_destroy;
  impl_class->beep = gdk_wayland_surface_beep;

  impl_class->destroy_notify = gdk_wayland_surface_destroy_notify;
  impl_class->drag_begin = _gdk_wayland_surface_drag_begin;
  impl_class->get_scale_factor = gdk_wayland_surface_get_scale_factor;
  impl_class->set_opaque_region = gdk_wayland_surface_set_opaque_region;
  impl_class->request_layout = gdk_wayland_surface_request_layout;
  impl_class->compute_size = gdk_wayland_surface_compute_size;
}

void
_gdk_wayland_surface_set_grab_seat (GdkSurface *surface,
                                    GdkSeat    *seat)
{
  GdkWaylandSurface *impl;

  g_return_if_fail (surface != NULL);

  impl = GDK_WAYLAND_SURFACE (surface);
  impl->grab_input_seat = seat;
}

/**
 * gdk_wayland_surface_get_wl_surface: (skip)
 * @surface: (type GdkWaylandSurface): a `GdkSurface`
 *
 * Returns the Wayland `wl_surface` of a `GdkSurface`.
 *
 * Returns: (transfer none): a Wayland `wl_surface`
 */
struct wl_surface *
gdk_wayland_surface_get_wl_surface (GdkSurface *surface)
{
  g_return_val_if_fail (GDK_IS_WAYLAND_SURFACE (surface), NULL);

  return GDK_WAYLAND_SURFACE (surface)->display_server.wl_surface;
}

struct wl_output *
gdk_wayland_surface_get_wl_output (GdkSurface *surface)
{
  GdkWaylandSurface *impl;

  g_return_val_if_fail (GDK_IS_WAYLAND_SURFACE (surface), NULL);

  impl = GDK_WAYLAND_SURFACE (surface);
  /* We pick the head of the list as this is the last entered output */
  if (impl->display_server.outputs)
    return (struct wl_output *) impl->display_server.outputs->data;

  return NULL;
}

void
gdk_wayland_surface_ensure_wl_egl_window (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  if (impl->display_server.egl_window == NULL)
    {
      impl->display_server.egl_window =
        wl_egl_window_create (impl->display_server.wl_surface,
                              surface->width * impl->scale,
                              surface->height * impl->scale);
      wl_surface_set_buffer_scale (impl->display_server.wl_surface, impl->scale);

      gdk_surface_set_egl_native_window (surface, impl->display_server.egl_window);
    }
}

struct gtk_surface1 *
gdk_wayland_surface_get_gtk_surface (GdkSurface *surface)
{
  g_return_val_if_fail (GDK_IS_WAYLAND_SURFACE (surface), NULL);

  return GDK_WAYLAND_SURFACE (surface)->display_server.gtk_surface;
}

static void
maybe_set_gtk_surface_dbus_properties (GdkWaylandSurface *impl)
{
  if (impl->application.was_set)
    return;

  if (impl->application.application_id == NULL &&
      impl->application.app_menu_path == NULL &&
      impl->application.menubar_path == NULL &&
      impl->application.window_object_path == NULL &&
      impl->application.application_object_path == NULL &&
      impl->application.unique_bus_name == NULL)
    return;

  gdk_wayland_surface_init_gtk_surface (impl);
  if (impl->display_server.gtk_surface == NULL)
    return;

  gtk_surface1_set_dbus_properties (impl->display_server.gtk_surface,
                                    impl->application.application_id,
                                    impl->application.app_menu_path,
                                    impl->application.menubar_path,
                                    impl->application.window_object_path,
                                    impl->application.application_object_path,
                                    impl->application.unique_bus_name);
  impl->application.was_set = TRUE;
}

void
gdk_wayland_toplevel_set_dbus_properties (GdkToplevel *toplevel,
                                          const char  *application_id,
                                          const char  *app_menu_path,
                                          const char  *menubar_path,
                                          const char  *window_object_path,
                                          const char  *application_object_path,
                                          const char *unique_bus_name)
{
  GdkWaylandSurface *impl;

  g_return_if_fail (GDK_IS_WAYLAND_TOPLEVEL (toplevel));

  impl = GDK_WAYLAND_SURFACE (toplevel);

  impl->application.application_id = g_strdup (application_id);
  impl->application.app_menu_path = g_strdup (app_menu_path);
  impl->application.menubar_path = g_strdup (menubar_path);
  impl->application.window_object_path = g_strdup (window_object_path);
  impl->application.application_object_path =
    g_strdup (application_object_path);
  impl->application.unique_bus_name = g_strdup (unique_bus_name);

  maybe_set_gtk_surface_dbus_properties (impl);
}

void
_gdk_wayland_surface_offset_next_wl_buffer (GdkSurface *surface,
                                            int         x,
                                            int         y)
{
  GdkWaylandSurface *impl;

  g_return_if_fail (GDK_IS_WAYLAND_SURFACE (surface));

  impl = GDK_WAYLAND_SURFACE (surface);

  impl->pending_buffer_offset_x = x;
  impl->pending_buffer_offset_y = y;
}

static void
xdg_exported_handle (void                    *data,
                     struct zxdg_exported_v1 *zxdg_exported_v1,
                     const char              *handle)
{
  GdkToplevel *toplevel = data;
  GdkWaylandToplevel *wayland_toplevel = GDK_WAYLAND_TOPLEVEL (toplevel);

  wayland_toplevel->exported.callback (toplevel, handle, wayland_toplevel->exported.user_data);
  if (wayland_toplevel->exported.destroy_func)
    {
      g_clear_pointer (&wayland_toplevel->exported.user_data,
                       wayland_toplevel->exported.destroy_func);
    }
}

static const struct zxdg_exported_v1_listener xdg_exported_listener = {
  xdg_exported_handle
};

static void
xdg_exported_handle2 (void                    *data,
                      struct zxdg_exported_v2 *zxdg_exported_v2,
                      const char              *handle)
{
  GdkToplevel *toplevel = data;
  GdkWaylandToplevel *wayland_toplevel = GDK_WAYLAND_TOPLEVEL (toplevel);

  wayland_toplevel->exported.callback (toplevel, handle, wayland_toplevel->exported.user_data);
  if (wayland_toplevel->exported.destroy_func)
    {
      g_clear_pointer (&wayland_toplevel->exported.user_data,
                       wayland_toplevel->exported.destroy_func);
    }
}

static const struct zxdg_exported_v2_listener xdg_exported_listener2 = {
  xdg_exported_handle2
};

/**
 * GdkWaylandToplevelExported:
 * @toplevel: (type GdkWaylandToplevel): the `GdkToplevel` that is exported
 * @handle: the handle
 * @user_data: user data that was passed to [method@GdkWayland.WaylandToplevel.export_handle]
 *
 * Callback that gets called when the handle for a surface has been
 * obtained from the Wayland compositor.
 *
 * This callback is used in [method@GdkWayland.WaylandToplevel.export_handle].
 *
 * The @handle can be passed to other processes, for the purpose of
 * marking surfaces as transient for out-of-process surfaces.
 */

static gboolean
gdk_wayland_toplevel_is_exported (GdkWaylandToplevel *wayland_toplevel)
{
  return !!wayland_toplevel->xdg_exported;
}

/**
 * gdk_wayland_toplevel_export_handle:
 * @toplevel: (type GdkWaylandToplevel): the `GdkToplevel` to obtain a handle for
 * @callback: callback to call with the handle
 * @user_data: (closure): user data for @callback
 * @destroy_func: destroy notify for @user_data
 *
 * Asynchronously obtains a handle for a surface that can be passed
 * to other processes.
 *
 * When the handle has been obtained, @callback will be called.
 *
 * It is an error to call this function on a surface that is already
 * exported.
 *
 * When the handle is no longer needed, [method@GdkWayland.WaylandToplevel.unexport_handle]
 * should be called to clean up resources.
 *
 * The main purpose for obtaining a handle is to mark a surface
 * from another surface as transient for this one, see
 * [method@GdkWayland.WaylandToplevel.set_transient_for_exported].
 *
 * Note that this API depends on an unstable Wayland protocol,
 * and thus may require changes in the future.
 *
 * Return value: %TRUE if the handle has been requested, %FALSE if
 *   an error occurred.
 */
gboolean
gdk_wayland_toplevel_export_handle (GdkToplevel                *toplevel,
                                    GdkWaylandToplevelExported  callback,
                                    gpointer                    user_data,
                                    GDestroyNotify              destroy_func)
{
  GdkWaylandToplevel *wayland_toplevel;
  GdkSurface *surface;
  GdkWaylandDisplay *display_wayland;
  GdkDisplay *display = gdk_surface_get_display (GDK_SURFACE (toplevel));

  g_return_val_if_fail (GDK_IS_WAYLAND_TOPLEVEL (toplevel), FALSE);
  g_return_val_if_fail (GDK_IS_WAYLAND_DISPLAY (display), FALSE);

  wayland_toplevel = GDK_WAYLAND_TOPLEVEL (toplevel);
  surface = GDK_SURFACE (toplevel);
  display_wayland = GDK_WAYLAND_DISPLAY (display);

  g_return_val_if_fail (!wayland_toplevel->xdg_exported, FALSE);

  if (display_wayland->xdg_exporter_v2)
    {
      struct zxdg_exported_v2 *xdg_exported;
      xdg_exported =
        zxdg_exporter_v2_export_toplevel (display_wayland->xdg_exporter_v2,
                                          gdk_wayland_surface_get_wl_surface (surface));
      zxdg_exported_v2_add_listener (xdg_exported, &xdg_exported_listener2, wayland_toplevel);
      wayland_toplevel->xdg_exported_v2 = xdg_exported;
    }
  else if (display_wayland->xdg_exporter)
    {
      struct zxdg_exported_v1 *xdg_exported;
      xdg_exported =
        zxdg_exporter_v1_export (display_wayland->xdg_exporter,
                                 gdk_wayland_surface_get_wl_surface (surface));
      zxdg_exported_v1_add_listener (xdg_exported, &xdg_exported_listener, wayland_toplevel);
      wayland_toplevel->xdg_exported = xdg_exported;
    }
  else
    {
      g_warning ("Server is missing xdg_foreign support");
      return FALSE;
    }

  wayland_toplevel->exported.callback = callback;
  wayland_toplevel->exported.user_data = user_data;
  wayland_toplevel->exported.destroy_func = destroy_func;

  return TRUE;
}

/**
 * gdk_wayland_toplevel_unexport_handle:
 * @toplevel: (type GdkWaylandToplevel): the `GdkToplevel` to unexport
 *
 * Destroys the handle that was obtained with
 * gdk_wayland_toplevel_export_handle().
 *
 * It is an error to call this function on a surface that
 * does not have a handle.
 *
 * Note that this API depends on an unstable Wayland protocol,
 * and thus may require changes in the future.
 */
void
gdk_wayland_toplevel_unexport_handle (GdkToplevel *toplevel)
{
  GdkWaylandToplevel *wayland_toplevel;

  g_return_if_fail (GDK_IS_WAYLAND_TOPLEVEL (toplevel));

  wayland_toplevel = GDK_WAYLAND_TOPLEVEL (toplevel);

  g_return_if_fail (wayland_toplevel->xdg_exported || wayland_toplevel->xdg_exported_v2);

  g_clear_pointer (&wayland_toplevel->xdg_exported, zxdg_exported_v1_destroy);
  g_clear_pointer (&wayland_toplevel->xdg_exported_v2, zxdg_exported_v2_destroy);
  if (wayland_toplevel->exported.destroy_func)
    {
      g_clear_pointer (&wayland_toplevel->exported.user_data,
                       wayland_toplevel->exported.destroy_func);
    }
}

static void
unset_transient_for_exported (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  g_clear_pointer (&impl->imported_transient_for, zxdg_imported_v1_destroy);
}

static void
xdg_imported_destroyed (void                    *data,
                        struct zxdg_imported_v1 *zxdg_imported_v1)
{
  GdkSurface *surface = data;

  unset_transient_for_exported (surface);
}

static const struct zxdg_imported_v1_listener xdg_imported_listener = {
  xdg_imported_destroyed,
};

/**
 * gdk_wayland_toplevel_set_transient_for_exported:
 * @toplevel: (type GdkWaylandToplevel): the `GdkToplevel` to make as transient
 * @parent_handle_str: an exported handle for a surface
 *
 * Marks @toplevel as transient for the surface to which the given
 * @parent_handle_str refers.
 *
 * Typically, the handle will originate from a
 * [method@GdkWayland.WaylandToplevel.export_handle] call in another process.
 *
 * Note that this API depends on an unstable Wayland protocol,
 * and thus may require changes in the future.
 *
 * Return value: %TRUE if the surface has been marked as transient,
 *   %FALSE if an error occurred.
 */
gboolean
gdk_wayland_toplevel_set_transient_for_exported (GdkToplevel *toplevel,
                                                 const char  *parent_handle_str)
{
  GdkWaylandSurface *impl;
  GdkWaylandDisplay *display_wayland;
  GdkDisplay *display = gdk_surface_get_display (GDK_SURFACE (toplevel));

  g_return_val_if_fail (GDK_IS_WAYLAND_TOPLEVEL (toplevel), FALSE);
  g_return_val_if_fail (GDK_IS_WAYLAND_DISPLAY (display), FALSE);

  impl = GDK_WAYLAND_SURFACE (toplevel);
  display_wayland = GDK_WAYLAND_DISPLAY (display);

  if (!display_wayland->xdg_importer)
    {
      g_warning ("Server is missing xdg_foreign support");
      return FALSE;
    }

  gdk_wayland_toplevel_set_transient_for (GDK_WAYLAND_TOPLEVEL (impl), NULL);

  impl->imported_transient_for =
    zxdg_importer_v1_import (display_wayland->xdg_importer, parent_handle_str);
  zxdg_imported_v1_add_listener (impl->imported_transient_for,
                                 &xdg_imported_listener,
                                 toplevel);

  gdk_wayland_surface_sync_parent_of_imported (impl);

  return TRUE;
}

static struct zwp_keyboard_shortcuts_inhibitor_v1 *
gdk_wayland_surface_get_inhibitor (GdkWaylandSurface *impl,
                                   GdkSeat           *gdk_seat)
{
  return g_hash_table_lookup (impl->shortcuts_inhibitors, gdk_seat);
}

/*
 * gdk_wayland_surface_inhibit_shortcuts:
 * @surface: (type GdkWaylandSurface): a `GdkSurface`
 * @seat: the seat to inhibit
 *
 * Inhibits the shortcuts coming from the given @seat.
 */
void
gdk_wayland_surface_inhibit_shortcuts (GdkSurface *surface,
                                       GdkSeat    *gdk_seat)
{
  GdkWaylandSurface *impl= GDK_WAYLAND_SURFACE (surface);
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));
  struct wl_surface *wl_surface = impl->display_server.wl_surface;
  struct wl_seat *seat = gdk_wayland_seat_get_wl_seat (gdk_seat);
  struct zwp_keyboard_shortcuts_inhibitor_v1 *inhibitor;

  if (display->keyboard_shortcuts_inhibit == NULL)
    return;

  if (gdk_wayland_surface_get_inhibitor (impl, gdk_seat))
    return; /* Already inhibited */

  inhibitor =
      zwp_keyboard_shortcuts_inhibit_manager_v1_inhibit_shortcuts (
          display->keyboard_shortcuts_inhibit, wl_surface, seat);

  g_hash_table_insert (impl->shortcuts_inhibitors, gdk_seat, inhibitor);
}

/*
 * gdk_wayland_surface_restore_shortcuts:
 * @surface: (type GdkWaylandSurface): a `GdkSurface`
 * @seat: the seat to inhibit
 *
 * Restores the shortcuts on the given @seat inhibited by calling
 * gdk_wayland_surface_inhibit_shortcuts().
 */
void
gdk_wayland_surface_restore_shortcuts (GdkSurface *surface,
                                       GdkSeat    *gdk_seat)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  struct zwp_keyboard_shortcuts_inhibitor_v1 *inhibitor;

  inhibitor = gdk_wayland_surface_get_inhibitor (impl, gdk_seat);
  if (inhibitor == NULL)
    return; /* Not inhibitted */

  zwp_keyboard_shortcuts_inhibitor_v1_destroy (inhibitor);
  g_hash_table_remove (impl->shortcuts_inhibitors, gdk_seat);
}

GdkSurface *
create_dnd_surface (GdkDisplay *display)
{
  GdkSurface *surface;

  surface = _gdk_wayland_display_create_surface (display,
                                                 GDK_SURFACE_TEMP,
                                                 NULL,
                                                 0, 0, 100, 100);

  return surface;
}

#define LAST_PROP 1

static void
gdk_wayland_popup_init (GdkWaylandPopup *popup)
{
}

static void
gdk_wayland_popup_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GdkSurface *surface = GDK_SURFACE (object);

  switch (prop_id)
    {
    case LAST_PROP + GDK_POPUP_PROP_PARENT:
      g_value_set_object (value, surface->parent);
      break;

    case LAST_PROP + GDK_POPUP_PROP_AUTOHIDE:
      g_value_set_boolean (value, surface->autohide);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_wayland_popup_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GdkSurface *surface = GDK_SURFACE (object);

  switch (prop_id)
    {
    case LAST_PROP + GDK_POPUP_PROP_PARENT:
      surface->parent = g_value_dup_object (value);
      if (surface->parent != NULL)
        surface->parent->children = g_list_prepend (surface->parent->children, surface);
      break;

    case LAST_PROP + GDK_POPUP_PROP_AUTOHIDE:
      surface->autohide = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_wayland_popup_class_init (GdkWaylandPopupClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->get_property = gdk_wayland_popup_get_property;
  object_class->set_property = gdk_wayland_popup_set_property;

  gdk_popup_install_properties (object_class, 1);
}

static gboolean
gdk_wayland_popup_present (GdkPopup       *popup,
                           int             width,
                           int             height,
                           GdkPopupLayout *layout)
{
  return gdk_wayland_surface_present_popup (GDK_SURFACE (popup), width, height, layout);
}

static GdkGravity
gdk_wayland_popup_get_surface_anchor (GdkPopup *popup)
{
  return GDK_SURFACE (popup)->popup.surface_anchor;
}

static GdkGravity
gdk_wayland_popup_get_rect_anchor (GdkPopup *popup)
{
  return GDK_SURFACE (popup)->popup.rect_anchor;
}

static int
gdk_wayland_popup_get_position_x (GdkPopup *popup)
{
  return GDK_SURFACE (popup)->x;
}

static int
gdk_wayland_popup_get_position_y (GdkPopup *popup)
{
  return GDK_SURFACE (popup)->y;
}

static void
gdk_wayland_popup_iface_init (GdkPopupInterface *iface)
{
  iface->present = gdk_wayland_popup_present;
  iface->get_surface_anchor = gdk_wayland_popup_get_surface_anchor;
  iface->get_rect_anchor = gdk_wayland_popup_get_rect_anchor;
  iface->get_position_x = gdk_wayland_popup_get_position_x;
  iface->get_position_y = gdk_wayland_popup_get_position_y;
}

static void
gdk_wayland_toplevel_init (GdkWaylandToplevel *toplevel)
{
}

static void
gdk_wayland_toplevel_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GdkSurface *surface = GDK_SURFACE (object);
  GdkWaylandToplevel *toplevel = GDK_WAYLAND_TOPLEVEL (surface);

  switch (prop_id)
    {
    case LAST_PROP + GDK_TOPLEVEL_PROP_TITLE:
      gdk_wayland_surface_set_title (surface, g_value_get_string (value));
      g_object_notify_by_pspec (G_OBJECT (surface), pspec);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_STARTUP_ID:
      gdk_wayland_surface_set_startup_id (surface, g_value_get_string (value));
      g_object_notify_by_pspec (G_OBJECT (surface), pspec);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_TRANSIENT_FOR:
      gdk_wayland_toplevel_set_transient_for (toplevel,
                                              g_value_get_object (value));
      g_object_notify_by_pspec (G_OBJECT (surface), pspec);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_MODAL:
      gdk_wayland_surface_set_modal_hint (surface, g_value_get_boolean (value));
      g_object_notify_by_pspec (G_OBJECT (surface), pspec);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_ICON_LIST:
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_DECORATED:
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_DELETABLE:
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_FULLSCREEN_MODE:
      surface->fullscreen_mode = g_value_get_enum (value);
      g_object_notify_by_pspec (G_OBJECT (surface), pspec);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_SHORTCUTS_INHIBITED:
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_wayland_toplevel_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GdkSurface *surface = GDK_SURFACE (object);
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  GdkWaylandToplevel *toplevel = GDK_WAYLAND_TOPLEVEL (surface);

  switch (prop_id)
    {
    case LAST_PROP + GDK_TOPLEVEL_PROP_STATE:
      g_value_set_flags (value, surface->state);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_TITLE:
      g_value_set_string (value, impl->title);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_STARTUP_ID:
      g_value_set_string (value, "");
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_TRANSIENT_FOR:
      g_value_set_object (value, toplevel->transient_for);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_MODAL:
      g_value_set_boolean (value, surface->modal_hint);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_ICON_LIST:
      g_value_set_pointer (value, NULL);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_DECORATED:
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_DELETABLE:
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_FULLSCREEN_MODE:
      g_value_set_enum (value, surface->fullscreen_mode);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_SHORTCUTS_INHIBITED:
      g_value_set_boolean (value, surface->shortcuts_inhibited);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_wayland_toplevel_finalize (GObject *object)
{
  GdkWaylandToplevel *wayland_toplevel;

  g_return_if_fail (GDK_IS_WAYLAND_TOPLEVEL (object));

  wayland_toplevel = GDK_WAYLAND_TOPLEVEL (object);

  if (gdk_wayland_toplevel_is_exported (wayland_toplevel))
    gdk_wayland_toplevel_unexport_handle (GDK_TOPLEVEL (wayland_toplevel));

  G_OBJECT_CLASS (gdk_wayland_toplevel_parent_class)->finalize (object);
}

static void
gdk_wayland_toplevel_class_init (GdkWaylandToplevelClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->get_property = gdk_wayland_toplevel_get_property;
  object_class->set_property = gdk_wayland_toplevel_set_property;
  object_class->finalize = gdk_wayland_toplevel_finalize;

  gdk_toplevel_install_properties (object_class, 1);
}

static void
gdk_wayland_toplevel_present (GdkToplevel       *toplevel,
                              GdkToplevelLayout *layout)
{
  GdkSurface *surface = GDK_SURFACE (toplevel);
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  gboolean pending_configure = FALSE;
  gboolean maximize;
  gboolean fullscreen;

  if (gdk_toplevel_layout_get_maximized (layout, &maximize))
    {
      if (maximize)
        gdk_wayland_surface_maximize (surface);
      else
        gdk_wayland_surface_unmaximize (surface);
      pending_configure = TRUE;
    }

  if (gdk_toplevel_layout_get_fullscreen (layout, &fullscreen))
    {
      if (fullscreen)
        {
          GdkMonitor *monitor;

          monitor = gdk_toplevel_layout_get_fullscreen_monitor (layout);
          if (monitor)
            gdk_wayland_surface_fullscreen_on_monitor (surface, monitor);
          else
            gdk_wayland_surface_fullscreen (surface);
        }
      else
        {
          gdk_wayland_surface_unfullscreen (surface);
        }
      pending_configure = TRUE;
    }

  g_clear_pointer (&impl->toplevel.layout, gdk_toplevel_layout_unref);
  impl->toplevel.layout = gdk_toplevel_layout_copy (layout);

  gdk_wayland_surface_show (surface);

  if (!pending_configure)
    {
      impl->next_layout.surface_geometry_dirty = TRUE;
      gdk_surface_request_layout (surface);
    }
}

static gboolean
gdk_wayland_toplevel_minimize (GdkToplevel *toplevel)
{
  gdk_wayland_surface_minimize (GDK_SURFACE (toplevel));

  return TRUE;
}

static gboolean
gdk_wayland_toplevel_lower (GdkToplevel *toplevel)
{
  return FALSE;
}

static void
gdk_wayland_toplevel_focus (GdkToplevel *toplevel,
                            guint32      timestamp)
{
  gdk_wayland_surface_focus (GDK_SURFACE (toplevel), timestamp);
}

static gboolean
gdk_wayland_toplevel_show_window_menu (GdkToplevel *toplevel,
                                       GdkEvent    *event)
{
  return gdk_wayland_surface_show_window_menu (GDK_SURFACE (toplevel), event);
}

static gboolean
gdk_wayland_toplevel_titlebar_gesture (GdkToplevel        *toplevel,
                                       GdkTitlebarGesture  gesture)
{
  return gdk_wayland_surface_titlebar_gesture (GDK_SURFACE (toplevel), gesture);
}

static gboolean
gdk_wayland_toplevel_supports_edge_constraints (GdkToplevel *toplevel)
{
  return gdk_wayland_surface_supports_edge_constraints (GDK_SURFACE (toplevel));
}

static void
inhibitor_active (void *data,
                  struct zwp_keyboard_shortcuts_inhibitor_v1 *inhibitor)
{
  GdkToplevel *toplevel = GDK_TOPLEVEL (data);
  GdkSurface *surface = GDK_SURFACE (toplevel);

  surface->shortcuts_inhibited = TRUE;
  g_object_notify (G_OBJECT (toplevel), "shortcuts-inhibited");
}

static void
inhibitor_inactive (void *data,
                    struct zwp_keyboard_shortcuts_inhibitor_v1 *inhibitor)
{
  GdkToplevel *toplevel = GDK_TOPLEVEL (data);
  GdkSurface *surface = GDK_SURFACE (toplevel);

  surface->shortcuts_inhibited = FALSE;
  g_object_notify (G_OBJECT (toplevel), "shortcuts-inhibited");
}

static const struct zwp_keyboard_shortcuts_inhibitor_v1_listener
zwp_keyboard_shortcuts_inhibitor_listener = {
  inhibitor_active,
  inhibitor_inactive,
};

static void
gdk_wayland_toplevel_inhibit_system_shortcuts (GdkToplevel *toplevel,
                                               GdkEvent    *event)
{
  struct zwp_keyboard_shortcuts_inhibitor_v1 *inhibitor;
  GdkSurface *surface = GDK_SURFACE (toplevel);
  GdkWaylandSurface *impl= GDK_WAYLAND_SURFACE (surface);
  GdkSeat *gdk_seat;

  if (surface->shortcuts_inhibited)
    return;

  gdk_seat = gdk_surface_get_seat_from_event (surface, event);
  gdk_wayland_surface_inhibit_shortcuts (surface, gdk_seat);

  inhibitor = gdk_wayland_surface_get_inhibitor (impl, gdk_seat);
  if (!inhibitor)
    return;

  surface->current_shortcuts_inhibited_seat = gdk_seat;
  zwp_keyboard_shortcuts_inhibitor_v1_add_listener
    (inhibitor, &zwp_keyboard_shortcuts_inhibitor_listener, toplevel);
}

static void
gdk_wayland_toplevel_restore_system_shortcuts (GdkToplevel *toplevel)
{
  GdkSurface *surface = GDK_SURFACE (toplevel);

  gdk_wayland_surface_restore_shortcuts (surface,
                                         surface->current_shortcuts_inhibited_seat);
  surface->current_shortcuts_inhibited_seat = NULL;
  surface->shortcuts_inhibited = FALSE;
  g_object_notify (G_OBJECT (toplevel), "shortcuts-inhibited");
}

static void
xdg_exported_handle_v1 (void                    *data,
                        struct zxdg_exported_v1 *zxdg_exported_v1,
                        const char              *handle)
{
  g_task_return_pointer (G_TASK (data), g_strdup (handle), g_free);
  g_object_unref (data);
}

static const struct zxdg_exported_v1_listener xdg_exported_listener_v1 = {
  xdg_exported_handle_v1
};

static void
xdg_exported_handle_v2 (void                    *data,
                        struct zxdg_exported_v2 *zxdg_exported_v2,
                        const char              *handle)
{
  g_task_return_pointer (G_TASK (data), g_strdup (handle), g_free);
  g_object_unref (data);
}

static const struct zxdg_exported_v2_listener xdg_exported_listener_v2 = {
  xdg_exported_handle_v2
};

static void
gdk_wayland_toplevel_real_export_handle (GdkToplevel          *toplevel,
                                         GCancellable         *cancellable,
                                         GAsyncReadyCallback   callback,
                                         gpointer              user_data)
{
  GdkWaylandToplevel *wayland_toplevel = GDK_WAYLAND_TOPLEVEL (toplevel);
  GdkSurface *surface = GDK_SURFACE (toplevel);
  GdkDisplay *display = gdk_surface_get_display (GDK_SURFACE (toplevel));
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);
  GTask *task;

  task = g_task_new (toplevel, cancellable, callback, user_data);

  if (display_wayland->xdg_exporter_v2)
    {
      wayland_toplevel->xdg_exported_v2 =
        zxdg_exporter_v2_export_toplevel (display_wayland->xdg_exporter_v2,
                                          gdk_wayland_surface_get_wl_surface (surface));
      zxdg_exported_v2_add_listener (wayland_toplevel->xdg_exported_v2,
                                     &xdg_exported_listener_v2, task);
    }
  else if (display_wayland->xdg_exporter)
    {
      wayland_toplevel->xdg_exported =
        zxdg_exporter_v1_export (display_wayland->xdg_exporter,
                                 gdk_wayland_surface_get_wl_surface (surface));
      zxdg_exported_v1_add_listener (wayland_toplevel->xdg_exported,
                                     &xdg_exported_listener_v1, task);
    }
  else
    {
      g_task_return_pointer (task, NULL, NULL);
      g_object_unref (task);
      return;
    }
}

static char *
gdk_wayland_toplevel_real_export_handle_finish (GdkToplevel   *toplevel,
                                                GAsyncResult  *result,
                                                GError       **error)
{
  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
gdk_wayland_toplevel_real_unexport_handle (GdkToplevel *toplevel)
{
  GdkWaylandToplevel *wayland_toplevel = GDK_WAYLAND_TOPLEVEL (toplevel);
  GdkDisplay *display = gdk_surface_get_display (GDK_SURFACE (toplevel));
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);

  g_return_if_fail (GDK_IS_WAYLAND_TOPLEVEL (toplevel));

  if (display_wayland->xdg_exporter_v2)
    g_clear_pointer (&wayland_toplevel->xdg_exported_v2, zxdg_exported_v2_destroy);
  else if (display_wayland->xdg_exporter)
    g_clear_pointer (&wayland_toplevel->xdg_exported, zxdg_exported_v1_destroy);
}

static void
gdk_wayland_toplevel_iface_init (GdkToplevelInterface *iface)
{
  iface->present = gdk_wayland_toplevel_present;
  iface->minimize = gdk_wayland_toplevel_minimize;
  iface->lower = gdk_wayland_toplevel_lower;
  iface->focus = gdk_wayland_toplevel_focus;
  iface->show_window_menu = gdk_wayland_toplevel_show_window_menu;
  iface->titlebar_gesture = gdk_wayland_toplevel_titlebar_gesture;
  iface->supports_edge_constraints = gdk_wayland_toplevel_supports_edge_constraints;
  iface->inhibit_system_shortcuts = gdk_wayland_toplevel_inhibit_system_shortcuts;
  iface->restore_system_shortcuts = gdk_wayland_toplevel_restore_system_shortcuts;
  iface->begin_resize = gdk_wayland_toplevel_begin_resize;
  iface->begin_move = gdk_wayland_toplevel_begin_move;
  iface->export_handle = gdk_wayland_toplevel_real_export_handle;
  iface->export_handle_finish = gdk_wayland_toplevel_real_export_handle_finish;
  iface->unexport_handle = gdk_wayland_toplevel_real_unexport_handle;
}

static void
gdk_wayland_drag_surface_init (GdkWaylandDragSurface *surface)
{
}

static void
gdk_wayland_drag_surface_class_init (GdkWaylandDragSurfaceClass *class)
{
}

static gboolean
gdk_wayland_drag_surface_present (GdkDragSurface *drag_surface,
                                  int             width,
                                  int             height)
{
  GdkSurface *surface = GDK_SURFACE (drag_surface);
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  gdk_wayland_surface_show (surface);

  impl->next_layout.configured_width = width;
  impl->next_layout.configured_height = height;
  impl->next_layout.surface_geometry_dirty = TRUE;
  gdk_surface_request_layout (surface);

  maybe_notify_mapped (surface);

  return TRUE;
}

static void
gdk_wayland_drag_surface_iface_init (GdkDragSurfaceInterface *iface)
{
  iface->present = gdk_wayland_drag_surface_present;
}

